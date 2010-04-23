/*
   +----------------------------------------------------------------------------------------------+
   | Windows Cache for PHP                                                                        |
   +----------------------------------------------------------------------------------------------+
   | Copyright (c) 2009, Microsoft Corporation. All rights reserved.                              |
   |                                                                                              |
   | Redistribution and use in source and binary forms, with or without modification, are         |
   | permitted provided that the following conditions are met:                                    |
   | - Redistributions of source code must retain the above copyright notice, this list of        |
   | conditions and the following disclaimer.                                                     |
   | - Redistributions in binary form must reproduce the above copyright notice, this list of     |
   | conditions and the following disclaimer in the documentation and/or other materials provided |
   | with the distribution.                                                                       |
   | - Neither the name of the Microsoft Corporation nor the names of its contributors may be     |
   | used to endorse or promote products derived from this software without specific prior written|
   | permission.                                                                                  |
   |                                                                                              |
   | THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS  |
   | OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF              |
   | MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE   |
   | COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,    |
   | EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE|
   | GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED   |
   | AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING    |
   | NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED |
   | OF THE POSSIBILITY OF SUCH DAMAGE.                                                           |
   +----------------------------------------------------------------------------------------------+
   | Module: wincache_zvcache.c                                                                   |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

#define PER_RUN_SCAVENGE_COUNT       16

#ifndef IS_CONSTANT_TYPE_MASK
#define IS_CONSTANT_TYPE_MASK (~IS_CONSTANT_INDEX)
#endif

#define ZMALLOC(pcopy, size)         (pcopy->fnmalloc(pcopy->palloc, pcopy->hoffset, size))
#define ZREALLOC(pcopy, addr, size)  (pcopy->fnrealloc(pcopy->palloc, pcopy->hoffset, addr, size))
#define ZSTRDUP(pcopy, pstr)         (pcopy->fnstrdup(pcopy->palloc, pcopy->hoffset, pstr))
#define ZFREE(pcopy, addr)           (pcopy->fnfree(pcopy->palloc, pcopy->hoffset, addr))

#define ZOFFSET(pcopy, pbuffer)      ((pbuffer == NULL) ? 0 : (((char *)pbuffer) - (pcopy)->pbaseadr))
#define ZVALUE(pcopy, offset)        ((offset == 0) ? NULL : ((pcopy)->pbaseadr + (offset)))
#define ZVCACHE_VALUE(p, o)          ((zvcache_value *)alloc_get_cachevalue(p, o))

static int  copyin_zval(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zval * poriginal, zv_zval ** pcopied TSRMLS_DC);
static int  copyout_zval(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zv_zval * pcopied, zval ** poriginal TSRMLS_DC);
static int  copyin_hashtable(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, HashTable * poriginal, zv_HashTable ** pcopied TSRMLS_DC);
static int  copyin_bucket(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, Bucket * poriginal, zv_Bucket ** pcopied TSRMLS_DC);
static int  copyout_hashtable(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zv_HashTable * pcopied, HashTable ** poriginal TSRMLS_DC);
static int  copyout_bucket(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zv_Bucket * pcopied, Bucket ** poriginal TSRMLS_DC);

static int  find_zvcache_entry(zvcache_context * pcache, const char * key, unsigned int index, zvcache_value ** ppvalue);
static int  create_zvcache_data(zvcache_context * pcache, const char * key, zval * pzval, unsigned int ttl, zvcache_value ** ppvalue TSRMLS_DC);
static void destroy_zvcache_data(zvcache_context * pcache, zvcache_value * pvalue);
static void add_zvcache_entry(zvcache_context * pcache, unsigned int index, zvcache_value * pvalue);
static void remove_zvcache_entry(zvcache_context * pcache, unsigned int index, zvcache_value * pvalue);
static void run_zvcache_scavenger(zvcache_context * pcache);

/* Globals */
unsigned short gzvcacheid = 1;

/* Private functions */
static int copyin_zval(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zval * poriginal, zv_zval ** pcopied TSRMLS_DC)
{
    int                  result     = NONFATAL;
    zv_zval *            pnewzv     = NULL;
    zv_zval **           pfromht    = NULL;
    int                  allocated  = 0;
    char *               pbuffer    = NULL;
    int                  length     = 0;
    zv_HashTable *       phashtable = NULL;
    zvcopy_context *     phashcopy  = NULL;
    size_t               offset     = 0;
    php_serialize_data_t serdata    = {0};
    smart_str            smartstr   = {0};
    unsigned char        isfirst    = 0;

    dprintverbose("start copyin_zval");

    _ASSERT(pcache    != NULL);
    _ASSERT(pcopy     != NULL);
    _ASSERT(poriginal != NULL);
    _ASSERT(pcopied   != NULL);

    if(phtable != NULL && phtable->nTableSize)
    {
        /* Check if zval is already copied */
        /* Below won't work in 64-bit environment when pointer is 64-bit */
        if(zend_hash_index_find(phtable, (ulong)poriginal, (void **)&pfromht) == SUCCESS)
        {
            (*pfromht)->refcount__gc++;
#ifndef PHP_VERSION_52
            (*pfromht)->is_ref__gc = poriginal->is_ref__gc;
#else
            (*pfromht)->is_ref__gc = poriginal->is_ref;
#endif
            *pcopied = *pfromht;
            goto Finished;
        }
    }

    /* Allocate memory as required */
    if(*pcopied == NULL)
    {
        pnewzv = (zv_zval *)ZMALLOC(pcopy, sizeof(zv_zval));
        if(pnewzv == NULL)
        {
            result = pcopy->oomcode;
            goto Finished;
        }

        pcopy->allocsize += sizeof(zv_zval);
        allocated = 1;
    }
    else
    {
        pnewzv = *pcopied;
    }

    _ASSERT(pnewzv != NULL);

    if(phtable != NULL && phtable->nTableSize)
    {
        zend_hash_index_update(phtable, (ulong)poriginal, (void **)&pnewzv, sizeof(zv_zval *), NULL);
    }

    /* Set type and refcount */
#ifndef PHP_VERSION_52
    pnewzv->refcount__gc = poriginal->refcount__gc;
    pnewzv->is_ref__gc = poriginal->is_ref__gc;
#else
    pnewzv->refcount__gc = poriginal->refcount;
    pnewzv->is_ref__gc = poriginal->is_ref;
#endif
    pnewzv->type = poriginal->type;
    memset(&pnewzv->value, 0, sizeof(zv_zvalue_value));

    switch(poriginal->type & IS_CONSTANT_TYPE_MASK)
    {
        case IS_RESOURCE:
            _ASSERT(FALSE);
            break;

        case IS_NULL:
            /* Nothing to do */
            break;

        case IS_BOOL:
        case IS_LONG:
            pnewzv->value.lval = poriginal->value.lval;
            break;

        case IS_DOUBLE:
            pnewzv->value.dval = poriginal->value.dval;
            break;

        case IS_STRING:
        case IS_CONSTANT:
            if(poriginal->value.str.val)
            {
                length = poriginal->value.str.len + 1;
                
                pbuffer = (char *)ZMALLOC(pcopy, length);
                if(pbuffer == NULL)
                {
                    result = pcopy->oomcode;
                    goto Finished;
                }

                pcopy->allocsize += length;
                memcpy_s(pbuffer, length - 1, poriginal->value.str.val, length - 1);
                *(pbuffer + length - 1) = 0;
                pnewzv->value.str.val = ZOFFSET(pcopy, pbuffer);
            }

            pnewzv->value.str.len = poriginal->value.str.len;
            break;

        case IS_ARRAY:
        case IS_CONSTANT_ARRAY:
            /* Initialize zvcopied hashtable for direct call to copyin_zval */
            if(phtable == NULL)
            {
                phtable = WCG(zvcopied);
                isfirst = 1;
                zend_hash_init(phtable, 0, NULL, NULL, 0);
            }

            /* If pcopy is not set to use memory pool, use a different copy context */
            if(pcopy->hoffset == 0)
            {
                _ASSERT(pcache->incopy == pcopy);
                phashcopy = (zvcopy_context *)alloc_pemalloc(sizeof(zvcopy_context));
                if(phashcopy == NULL)
                {
                    result = FATAL_OUT_OF_LMEMORY;
                    goto Finished;
                }

                result = alloc_create_mpool(pcache->zvalloc, &offset);
                if(FAILED(result))
                {
                    goto Finished;
                }

                phashcopy->oomcode   = FATAL_OUT_OF_SMEMORY;
                phashcopy->palloc    = pcache->zvalloc;
                phashcopy->pbaseadr  = pcache->zvmemaddr;
                phashcopy->hoffset   = offset;
                phashcopy->allocsize = 0;
                phashcopy->fnmalloc  = alloc_ommalloc;
                phashcopy->fnrealloc = alloc_omrealloc;
                phashcopy->fnstrdup  = alloc_omstrdup;
                phashcopy->fnfree    = alloc_omfree;

                result = copyin_hashtable(pcache, phashcopy, phtable, poriginal->value.ht, &phashtable TSRMLS_CC);
                pcopy->allocsize += phashcopy->allocsize;
            }
            else
            {
                result = copyin_hashtable(pcache, pcopy, phtable, poriginal->value.ht, &phashtable TSRMLS_CC);
            }

            if(isfirst)
            {
                _ASSERT(phtable != NULL);

                zend_hash_destroy(phtable);
                phtable->nTableSize = 0;

                phtable = NULL;
            }

            if(FAILED(result))
            {
                goto Finished;
            }

            /* Keep offset so that freeing shared memory is easy */ 
            pnewzv->value.ht.hoff = offset;
            pnewzv->value.ht.val  = ZOFFSET(pcopy, phashtable);
            break;

        case IS_OBJECT:
            /* Serialize object and store in string */
            PHP_VAR_SERIALIZE_INIT(serdata);
            php_var_serialize(&smartstr, (zval **)&poriginal, &serdata TSRMLS_CC);
            PHP_VAR_SERIALIZE_DESTROY(serdata);

            if(smartstr.c)
            {
                pbuffer = (char *)ZMALLOC(pcopy, smartstr.len + 1);
                if(pbuffer == NULL)
                {
                    smart_str_free(&smartstr);

                    result = pcopy->oomcode;
                    goto Finished;
                }

                pcopy->allocsize += (smartstr.len + 1);
                memcpy_s(pbuffer, smartstr.len, smartstr.c, smartstr.len);
                *(pbuffer + smartstr.len) = 0;
                pnewzv->value.str.val = ZOFFSET(pcopy, pbuffer);
                pnewzv->value.str.len = smartstr.len;

                smart_str_free(&smartstr);
            }
            else
            {
                pnewzv->type = IS_NULL;
            }

            break;

        default:
            result = FATAL_ZVCACHE_INVALID_ZVAL;
            goto Finished;
    }

    *pcopied = pnewzv;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(phashcopy != NULL)
    {
        alloc_pefree(phashcopy);
        phashcopy = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_zval", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewzv != NULL)
        {
            if(allocated == 1)
            {
                ZFREE(pcopy, pnewzv);
                pnewzv = NULL;
            }

            if(offset != 0)
            {
                alloc_free_mpool(pcache->zvalloc, offset);
                offset = 0;
            }
        }
    }

    dprintverbose("end copyin_zval");

    return result;
}

static int copyout_zval(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zv_zval * pcopied, zval ** poriginal TSRMLS_DC)
{
    int                    result     = NONFATAL;
    zval *                 pnewzv     = NULL;
    zval **                pfromht    = NULL;
    int                    allocated  = 0;
    char *                 pbuffer    = NULL;
    int                    length     = 0;
    HashTable *            phashtable = NULL;
    php_unserialize_data_t serdata    = {0};
    unsigned char          isfirst    = 0;

    dprintverbose("start copyout_zval");

    _ASSERT(pcache    != NULL);
    _ASSERT(pcopy     != NULL);
    _ASSERT(pcopied   != NULL);
    _ASSERT(poriginal != NULL);

    if(phtable != NULL && phtable->nTableSize)
    {
        /* Check if zv_zval is already copied */
        /* Below won't work in 64-bit environment when pointer is 64-bit */
        if(zend_hash_index_find(phtable, (ulong)pcopied, (void **)&pfromht) == SUCCESS)
        {
#ifndef PHP_VERSION_52
            (*pfromht)->refcount__gc++;
            (*pfromht)->is_ref__gc = pcopied->is_ref__gc;
#else
            (*pfromht)->refcount++;
            (*pfromht)->is_ref = pcopied->is_ref__gc;
#endif
            *poriginal = *pfromht;
            goto Finished;
        }
    }

    /* Allocate memory as required */
    if(*poriginal == NULL)
    {
        ALLOC_ZVAL(pnewzv);
        if(pnewzv == NULL)
        {
            result = pcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewzv = *poriginal;
    }

    _ASSERT(pnewzv != NULL);

    if(phtable != NULL && phtable->nTableSize)
    {
        zend_hash_index_update(phtable, (ulong)pcopied, (void **)&pnewzv, sizeof(zval *), NULL);
    }

    /* Set type and refcount */
#ifndef PHP_VERSION_52
    pnewzv->refcount__gc = 1;
    pnewzv->is_ref__gc = 0;
#else
    pnewzv->refcount = 1;
    pnewzv->is_ref = 0;
#endif
    pnewzv->type = pcopied->type;
    memset(&pnewzv->value, 0, sizeof(zvalue_value));

    switch(pcopied->type & IS_CONSTANT_TYPE_MASK)
    {
        case IS_RESOURCE:
            _ASSERT(FALSE);
            break;

        case IS_NULL:
            /* Nothing to do */
            break;

        case IS_BOOL:
        case IS_LONG:
            pnewzv->value.lval = pcopied->value.lval;
            break;

        case IS_DOUBLE:
            pnewzv->value.dval = pcopied->value.dval;
            break;

        case IS_STRING:
        case IS_CONSTANT:
            if(pcopied->value.str.val > 0)
           {
                length = pcopied->value.str.len + 1;
                pbuffer = (char *)ZMALLOC(pcopy, length);

                if(pbuffer == NULL)
                {
                    result = pcopy->oomcode;
                    goto Finished;
                }

                memcpy_s(pbuffer, length - 1, ZVALUE(pcopy, pcopied->value.str.val), length - 1);
                *(pbuffer + length - 1) = 0;
                pnewzv->value.str.val = pbuffer;
            }

            pnewzv->value.str.len = pcopied->value.str.len;
            break;

        case IS_ARRAY:
        case IS_CONSTANT_ARRAY:
            /* Initialize zvcopied HashTable for first call to copyout_zval */
            if(phtable == NULL)
            {
                phtable = WCG(zvcopied);
                isfirst = 1;
                zend_hash_init(WCG(zvcopied), 0, NULL, NULL, 0);
            }
            
            result = copyout_hashtable(pcache, pcopy, phtable, (zv_HashTable *)ZVALUE(pcopy, pcopied->value.ht.val), &phashtable TSRMLS_CC);

            if(isfirst)
            {
                _ASSERT(phtable != NULL);

                zend_hash_destroy(phtable);
                phtable->nTableSize = 0;

                phtable = NULL;
            }

            if(FAILED(result))
            {
                goto Finished;
            }

            pnewzv->value.ht = phashtable;
            break;

        case IS_OBJECT:
            /* Deserialize stored data to produce object */
            pnewzv->type = IS_NULL;
            pbuffer = ZVALUE(pcopy, pcopied->value.str.val);

            PHP_VAR_UNSERIALIZE_INIT(serdata);

            if(!php_var_unserialize(&pnewzv, (const unsigned char **)&pbuffer, (const unsigned char *)pbuffer + pcopied->value.str.len, &serdata TSRMLS_CC))
            {
                /* Destroy zval and set return value to null */
                zval_dtor(pnewzv);
                pnewzv->type = IS_NULL;
            }

            PHP_VAR_UNSERIALIZE_DESTROY(serdata);
            break;

        default:
            result = FATAL_ZVCACHE_INVALID_ZVAL;
            goto Finished;
    }

    *poriginal = pnewzv;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyout_zval", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pbuffer != NULL)
        {
            ZFREE(pcopy, pbuffer);
            pbuffer = NULL;
        }

        if(pnewzv != NULL)
        {
            if(allocated == 1)
            {
                FREE_ZVAL(pnewzv);
                pnewzv = NULL;
            }
        }
    }

    dprintverbose("end copyout_zval");

    return result;
}

static int copyin_hashtable(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, HashTable * poriginal, zv_HashTable ** pcopied TSRMLS_DC)
{
    int            result    = NONFATAL;
    int            allocated = 0;
    unsigned int   index     = 0;
    unsigned int   bnum      = 0;

    zv_HashTable * pnewh    = NULL;
    char *         pbuffer  = NULL;
    Bucket *       pbucket  = NULL;
    size_t *       parb     = NULL;

    zv_Bucket *    pnewb    = NULL;
    zv_Bucket *    plast    = NULL;
    size_t         plastoff = 0;
    zv_Bucket *    ptemp    = NULL;
    size_t         ptempoff = 0;

    dprintverbose("start copyin_hashtable");

    _ASSERT(pcache    != NULL);
    _ASSERT(pcopy     != NULL);
    _ASSERT(poriginal != NULL);
    _ASSERT(pcopied   != NULL);

    /* Allocate memory for zv_HashTable as required */
    if(*pcopied == NULL)
    {
        pnewh = (zv_HashTable *)ZMALLOC(pcopy, sizeof(zv_HashTable));
        if(pnewh == NULL)
        {
            result = pcopy->oomcode;
            goto Finished;
        }

        pcopy->allocsize += sizeof(zv_HashTable);
        allocated = 1;
    }
    else
    {
        pnewh = *pcopied;
    }

    pnewh->nTableSize       = poriginal->nTableSize;
    pnewh->nTableMask       = poriginal->nTableMask;
    pnewh->nNumOfElements   = poriginal->nNumOfElements;
    pnewh->nNextFreeElement = poriginal->nNextFreeElement;
    pnewh->pDestructor      = NULL;
    pnewh->persistent       = poriginal->persistent;
    pnewh->nApplyCount      = poriginal->nApplyCount;
    pnewh->bApplyProtection = poriginal->bApplyProtection;
#if ZEND_DEBUG
    pnewh->inconsistent     = poriginal->inconsistent;
#endif

    pnewh->pInternalPointer = 0;
    pnewh->pListHead        = 0;
    pnewh->pListTail        = 0;
    pnewh->arBuckets        = 0;

    /* Allocate memory for buckets */
    pbuffer = (char *)ZMALLOC(pcopy, poriginal->nTableSize * sizeof(size_t));
    if(pbuffer == NULL)
    {
        result = pcopy->oomcode;
        goto Finished;
    }

    pcopy->allocsize += (poriginal->nTableSize * sizeof(size_t));

    memset(pbuffer, 0, poriginal->nTableSize * sizeof(size_t));
    pnewh->arBuckets = ZOFFSET(pcopy, pbuffer);

    /* Traverse the hashtable and copy the buckets */
    /* Using index as a boolean to set list head */
    index   = 0;
    pbucket = poriginal->pListHead;
    plast   = NULL;
    ptemp   = NULL;

    while(pbucket != NULL)
    {
        ptemp = NULL;

        /* Copy bucket containing zval_ref */
        result = copyin_bucket(pcache, pcopy, phtable, pbucket, &ptemp TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        ptempoff = ZOFFSET(pcopy, ptemp);

        if(index == 0)
        {
            pnewh->pListHead = ptempoff;
            index++;
        }

        ptemp->pListLast = plastoff;
        if(plast != NULL)
        {
            plast->pListNext = ptempoff;
        }

        bnum = ptemp->h % pnewh->nTableSize;
        parb = ((size_t *)ZVALUE(pcopy, pnewh->arBuckets)) + bnum;
        if(*parb != 0)
        {
            ptemp->pNext = *parb;
            ((zv_Bucket *)ZVALUE(pcopy, *parb))->pLast = ptempoff;
        }

        *parb = ptempoff;

        plast    = ptemp;
        plastoff = ptempoff;
        pbucket  = pbucket->pListNext;
    }

    *pcopied = pnewh;

    _ASSERT(SUCCEEDED(result));

Finished:
    
    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_hashtable", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pbuffer != NULL)
        {
            ZFREE(pcopy, pbuffer);
            pbuffer = NULL;
        }

        if(pnewh != NULL)
        {
            if(allocated != 0)
            {
                ZFREE(pcopy, pnewh);
                pnewh = NULL;
            }
        }
    }

    dprintverbose("end copyin_hashtable");

    return result;
}

static int copyin_bucket(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, Bucket * poriginal, zv_Bucket ** pcopied TSRMLS_DC)
{
    int         result    = NONFATAL;
    zv_Bucket * pnewb     = NULL;
    int         msize     = 0;
    int         allocated = 0;
    size_t *    pbuffer   = NULL;
    zv_zval *   pzval     = NULL;

    dprintverbose("start copyin_bucket");

    _ASSERT(pcache    != NULL);
    _ASSERT(pcopy     != NULL);
    _ASSERT(poriginal != NULL);
    _ASSERT(pcopied   != NULL);

    /* bucket key name is copied starting at last element of struct Bucket */
    msize = sizeof(zv_Bucket) + poriginal->nKeyLength - sizeof(char);

    /* Allocate memory if required */
    if(*pcopied == NULL)
    {
        pnewb = (zv_Bucket *)ZMALLOC(pcopy, msize);
        if(pnewb == NULL)
        {
            result = pcopy->oomcode;
            goto Finished;
        }

        pcopy->allocsize += msize;
        allocated = 1;
    }
    else
    {
        pnewb = *pcopied;
    }

    pnewb->h          = poriginal->h;
    pnewb->nKeyLength = poriginal->nKeyLength;
    pnewb->pData      = 0;
    pnewb->pDataPtr   = 0;
    pnewb->pListNext  = 0;
    pnewb->pListLast  = 0;
    pnewb->pNext      = 0;
    pnewb->pLast      = 0;

    memcpy_s(&pnewb->arKey, poriginal->nKeyLength, &poriginal->arKey, poriginal->nKeyLength);

    /* Do zval ref copy */
    pbuffer = (size_t *)ZMALLOC(pcopy, sizeof(size_t));
    if(pbuffer == NULL)
    {
        result = pcopy->oomcode;
        goto Finished;
    }

    pcopy->allocsize += sizeof(size_t);
    *pbuffer = 0;

    result = copyin_zval(pcache, pcopy, phtable, *((zval **)poriginal->pData), &pzval TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    *pbuffer     = ZOFFSET(pcopy, pzval);
    pnewb->pData = ZOFFSET(pcopy, pbuffer);

    /* No need to set pDataPtr */
    /* Just take care in copyout */

    *pcopied = pnewb;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_bucket", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pbuffer != NULL)
        {
            ZFREE(pcopy, pbuffer);
            pbuffer = NULL;
        }

        if(pnewb != NULL)
        {
            if(allocated != 0)
            {
                ZFREE(pcopy, pnewb);
                pnewb = NULL;
            }
        }
    }
    
    dprintverbose("end copyin_bucket");

    return result;
}

static int copyout_hashtable(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zv_HashTable * pcopied, HashTable ** poriginal TSRMLS_DC)
{
    int           result    = NONFATAL;
    int           allocated = 0;
    unsigned int  index     = 0;
    unsigned int  bnum      = 0;

    HashTable *   pnewh    = NULL;
    char *        pbuffer  = NULL;
    zv_Bucket *   pbucket  = NULL;

    Bucket *      pnewb    = NULL;
    Bucket *      plast    = NULL;
    size_t        plastoff = 0;
    Bucket *      ptemp    = NULL;
    size_t        ptempoff = 0;

    dprintverbose("start copyout_hashtable");

    _ASSERT(pcache    != NULL);
    _ASSERT(pcopy     != NULL);
    _ASSERT(pcopied   != NULL);
    _ASSERT(poriginal != NULL);

    /* Allocate memory for HashTable as required */
    if(*poriginal== NULL)
    {
        pnewh = (HashTable *)ZMALLOC(pcopy, sizeof(HashTable));
        if(pnewh == NULL)
        {
            result = pcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewh = *poriginal;
    }

    pnewh->nTableSize       = pcopied->nTableSize;
    pnewh->nTableMask       = pcopied->nTableMask;
    pnewh->nNumOfElements   = pcopied->nNumOfElements;
    pnewh->nNextFreeElement = pcopied->nNextFreeElement;
    pnewh->pDestructor      = NULL;
    pnewh->persistent       = pcopied->persistent;
    pnewh->nApplyCount      = pcopied->nApplyCount;
    pnewh->bApplyProtection = pcopied->bApplyProtection;
#if ZEND_DEBUG
    pnewh->inconsistent     = pcopied->inconsistent;
#endif

    pnewh->pInternalPointer = NULL;
    pnewh->pListHead        = NULL;
    pnewh->pListTail        = NULL;
    pnewh->arBuckets        = NULL;

    /* Allocate memory for buckets */
    pnewh->arBuckets = (Bucket **)ZMALLOC(pcopy, pcopied->nTableSize * sizeof(Bucket *));
    if(pnewh->arBuckets == NULL)
    {
        result = pcopy->oomcode;
        goto Finished;
    }

    for(index = 0; index < pcopied->nTableSize; index++)
    {
        pnewh->arBuckets[index] = NULL;
    }

    /* Traverse the hashtable and copy the buckets */
    /* Reusing index this time as a boolean to set list head */
    index   = 0;
    pbucket = (zv_Bucket *)ZVALUE(pcopy, pcopied->pListHead);
    plast   = NULL;
    ptemp   = NULL;

    while(pbucket != NULL)
    {
        ptemp = NULL;

        /* Copy bucket containing zval_ref */
        result = copyout_bucket(pcache, pcopy, phtable, pbucket, &ptemp TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        if(index == 0)
        {
            pnewh->pListHead = ptemp;
            index++;
        }

        ptemp->pListLast = plast;
        if(plast != NULL)
        {
            plast->pListNext = ptemp;
        }

        bnum = ptemp->h % pnewh->nTableSize;
        if(pnewh->arBuckets[bnum] != NULL)
        {
            ptemp->pNext = pnewh->arBuckets[bnum];
            ptemp->pNext->pLast = ptemp;
        }

        pnewh->arBuckets[bnum] = ptemp;

        plast   = ptemp;
        pbucket = (zv_Bucket *)ZVALUE(pcopy, pbucket->pListNext);
    }

    pnewh->pListTail = ptemp;

    *poriginal = pnewh;
    _ASSERT(SUCCEEDED(result));

Finished:
    
    if(FAILED(result))
    {
        dprintimportant("failure %d in copyout_hashtable", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewh != NULL)
        {
            if(pnewh->arBuckets != NULL)
            {
                ZFREE(pcopy, pnewh->arBuckets);
                pnewh->arBuckets = NULL;
            }

            if(allocated != 0)
            {
                ZFREE(pcopy, pnewh);
                pnewh = NULL;
            }
        }
    }

    dprintverbose("end copyout_hashtable");

    return result;
}

static int copyout_bucket(zvcache_context * pcache, zvcopy_context * pcopy, HashTable * phtable, zv_Bucket * pcopied, Bucket ** poriginal TSRMLS_DC)
{
    int       result    = NONFATAL;
    Bucket *  pnewb     = NULL;
    int       msize     = 0;
    int       allocated = 0;

    zval **   pbuffer   = NULL;
    zval *    pzval     = NULL;

    dprintverbose("start copyout_bucket");

    _ASSERT(pcache    != NULL);
    _ASSERT(pcopy     != NULL);
    _ASSERT(pcopied   != NULL);
    _ASSERT(poriginal != NULL);

    /* bucket key name is copied starting at last element of struct Bucket */
    msize = sizeof(Bucket) + pcopied->nKeyLength - sizeof(char);

    /* Allocate memory if required */
    if(*poriginal == NULL)
    {
        pnewb = (Bucket *)ZMALLOC(pcopy, msize);
        if(pnewb == NULL)
        {
            result = pcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewb = *poriginal;
    }

    pnewb->h          = pcopied->h;
    pnewb->nKeyLength = pcopied->nKeyLength;
    pnewb->pData      = NULL;
    pnewb->pDataPtr   = NULL;
    pnewb->pListNext  = NULL;
    pnewb->pListLast  = NULL;
    pnewb->pNext      = NULL;
    pnewb->pLast      = NULL;

    memcpy_s(&pnewb->arKey, pcopied->nKeyLength, &pcopied->arKey, pcopied->nKeyLength);

    /* Do zval ref copy */
    pbuffer = (zval **)ZMALLOC(pcopy, sizeof(zval *));
    if(pbuffer == NULL)
    {
        result = pcopy->oomcode;
        goto Finished;
    }

    *pbuffer = NULL;

    result = copyout_zval(pcache, pcopy, phtable, (zv_zval *)ZVALUE(pcopy, *((size_t *)ZVALUE(pcopy, pcopied->pData))), &pzval TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    *pbuffer        = pzval;
    pnewb->pData    = pbuffer;
    pnewb->pDataPtr = pzval;

    *poriginal = pnewb;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyout_bucket", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pbuffer != NULL)
        {
            ZFREE(pcopy, pbuffer);
            pbuffer = NULL;
        }

        if(pnewb != NULL)
        {
            if(allocated != 0)
            {
                ZFREE(pcopy, pnewb);
                pnewb = NULL;
            }
        }
    }
    
    dprintverbose("end copyout_bucket");

    return result;
}

/* Call this method atleast under a read lock */
static int find_zvcache_entry(zvcache_context * pcache, const char * key, unsigned int index, zvcache_value ** ppvalue)
{
    int              result = NONFATAL;
    zvcache_header * header = NULL;
    zvcache_value *  pvalue = NULL;
    unsigned int     tdiff  = 0;

    dprintverbose("start find_zvcache_entry");

    _ASSERT(pcache  != NULL);
    _ASSERT(key     != NULL);
    _ASSERT(ppvalue != NULL);

    *ppvalue  = NULL;

    header = pcache->zvheader;
    pvalue = ZVCACHE_VALUE(pcache->zvalloc, header->values[index]);

    while(pvalue != NULL)
    {
        if(strcmp(pcache->zvmemaddr + pvalue->keystr, key) == 0)
        {
            if(pvalue->ttlive > 0)
            {
                /* Check if the entry is not expired and */
                /* Stop looking only if entry is not stale */
                tdiff = utils_ticksdiff(0, pvalue->add_ticks) / 1000;
                if(tdiff <= pvalue->ttlive)
                {
                    break;
                }
            }
            else
            {
                /* ttlive 0 means entry is valid unless deleted */
                break;
            }
        }

        pvalue = ZVCACHE_VALUE(pcache->zvalloc, pvalue->next_value);
    }

    *ppvalue = pvalue;

    dprintverbose("end find_zvcache_entry");

    return result;
}

static int create_zvcache_data(zvcache_context * pcache, const char * key, zval * pzval, unsigned int ttl, zvcache_value ** ppvalue TSRMLS_DC)
{
    int              result  = NONFATAL;
    zvcache_value *  pvalue  = NULL;
    unsigned int     keylen  = 0;
    zv_zval *        pcopied = NULL;
    zvcopy_context * pcopy   = NULL;
    unsigned int     cticks  = 0;

    dprintverbose("start create_zvcache_data");

    _ASSERT(pcache   != NULL);
    _ASSERT(key      != NULL);
    _ASSERT(pzval    != NULL);
    _ASSERT(ppvalue  != NULL);
    _ASSERT(*ppvalue == NULL);

    *ppvalue = NULL;
    pcopy = pcache->incopy;

    keylen = strlen(key) + 1;
    _ASSERT(keylen < 4098);

    pvalue = (zvcache_value *)ZMALLOC(pcopy, sizeof(zvcache_value) + keylen);
    if(pvalue == NULL)
    {
        result = pcopy->oomcode;
        goto Finished;
    }

    /* Only set zvcache_value to 0. Set keyname after zvcache_value */
    memset(pvalue, 0, sizeof(zvcache_value));
    memcpy_s((char *)pvalue + sizeof(zvcache_value), keylen, key, keylen);

    /* Reset allocsize before calling copyin */
    pcopy->allocsize = 0;

    result = copyin_zval(pcache, pcopy, NULL, pzval, &pcopied TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    pvalue->keystr     = ZOFFSET(pcopy, pvalue) + sizeof(zvcache_value);
    pvalue->keylen     = (unsigned short)(keylen - 1);
    pvalue->sizeb      = pcopy->allocsize;
    pvalue->zvalue     = ZOFFSET(pcopy, pcopied);

    cticks = GetTickCount();
    pvalue->add_ticks  = cticks;
    pvalue->use_ticks  = cticks;

    pvalue->ttlive     = ttl;
    pvalue->hitcount   = 0;
    pvalue->next_value = 0;
    pvalue->prev_value = 0;

    *ppvalue = pvalue;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in create_zvcache_data", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pvalue != NULL)
        {
            ZFREE(pcopy, pvalue);
            pvalue = NULL;
        }
    }

    dprintverbose("end create_zvcache_data");

    return result;
}

static void destroy_zvcache_data(zvcache_context * pcache, zvcache_value * pvalue)
{
    zv_zval * pzval;

    dprintverbose("start destroy_zvcache_data");

    if(pvalue != NULL)
    {
        if(pvalue->zvalue != 0)
        {
            pzval = (zv_zval *)ZVALUE(pcache->incopy, pvalue->zvalue);

            switch(pzval->type & IS_CONSTANT_TYPE_MASK)
            {
                case IS_STRING:
                case IS_CONSTANT:
                case IS_OBJECT:
                    ZFREE(pcache->incopy, ZVALUE(pcache->incopy, pzval->value.str.val));
                    pzval->value.str.val = 0;
                    pzval->value.str.len = 0;
                    break;

                case IS_ARRAY:
                case IS_CONSTANT_ARRAY:
                    alloc_free_mpool(pcache->zvalloc, pzval->value.ht.hoff);
                    pzval->value.ht.val  = 0;
                    pzval->value.ht.hoff = 0;
                    break;
            }

            ZFREE(pcache->incopy, pzval);
            pvalue->zvalue = 0;
            pzval = NULL;
        }

        /* No need to call free for keystr as value and keystr are allocated together */
        ZFREE(pcache->incopy, pvalue);
        pvalue = NULL;
    }
    
    dprintverbose("end destroy_zvcache_data");

    return;
}

static void add_zvcache_entry(zvcache_context * pcache, unsigned int index, zvcache_value * pvalue)
{
    zvcache_header * header  = NULL;
    zvcache_value *  pcheck  = NULL;

    dprintverbose("start add_zvcache_entry");

    _ASSERT(pcache         != NULL);
    _ASSERT(pvalue         != NULL);
    _ASSERT(pvalue->keystr != 0);
    
    header = pcache->zvheader;
    pcheck = ZVCACHE_VALUE(pcache->zvalloc, header->values[index]);

    while(pcheck != NULL)
    {
        if(pcheck->next_value == 0)
        {
            break;
        }

        pcheck = ZVCACHE_VALUE(pcache->zvalloc, pcheck->next_value);
    }

    if(pcheck != NULL)
    {
        pcheck->next_value = alloc_get_valueoffset(pcache->zvalloc, pvalue);
        pvalue->next_value = 0;
        pvalue->prev_value = alloc_get_valueoffset(pcache->zvalloc, pcheck);
    }
    else
    {
        header->values[index] = alloc_get_valueoffset(pcache->zvalloc, pvalue);
        pvalue->next_value = 0;
        pvalue->prev_value = 0;
    }

    header->itemcount++;

    dprintverbose("end add_zvcache_entry");

    return;
}

/* Call this method under write lock */
static void remove_zvcache_entry(zvcache_context * pcache, unsigned int index, zvcache_value * pvalue)
{
    alloc_context *   apalloc = NULL;
    zvcache_header *  header  = NULL;
    zvcache_value *   ptemp   = NULL;

    dprintverbose("start remove_zvcache_entry");

    _ASSERT(pcache         != NULL);
    _ASSERT(pvalue         != NULL);
    _ASSERT(pvalue->keystr != 0);

    apalloc = pcache->zvalloc;
    header  = pcache->zvheader;

    /* Decrement itemcount and remove entry from hashtable */
    header->itemcount--;

    if(pvalue->prev_value == 0)
    {
        header->values[index] = pvalue->next_value;
        if(pvalue->next_value != 0)
        {
            ptemp = ZVCACHE_VALUE(apalloc, pvalue->next_value);
            ptemp->prev_value = 0;
        }
    }
    else
    {
        ptemp = ZVCACHE_VALUE(apalloc, pvalue->prev_value);
        ptemp->next_value = pvalue->next_value;

        if(pvalue->next_value != 0)
        {
            ptemp = ZVCACHE_VALUE(apalloc, pvalue->next_value);
            ptemp->prev_value = pvalue->prev_value;
        }
    }

    /* Destroy aplist data now that fcache and ocache is deleted */
    destroy_zvcache_data(pcache, pvalue);
    pvalue = NULL;
    
    dprintverbose("end remove_zvcache_entry");
    return;
}

/* Call this method under write lock */
static void run_zvcache_scavenger(zvcache_context * pcache)
{
    unsigned int sindex   = 0;
    unsigned int eindex   = 0;
    unsigned int ticks    = 0;
    unsigned int tickdiff = 0;

    zvcache_header * pheader = NULL;
    alloc_context *  palloc  = NULL;
    zvcache_value *  pvalue  = NULL;
    zvcache_value *  ptemp   = NULL;

    dprintverbose("start run_zvcache_scavenger");

    _ASSERT(pcache != NULL);

    pheader = pcache->zvheader;
    palloc  = pcache->zvalloc;
    ticks   = GetTickCount();

    /* Only run if lscavenge happened atleast 10 seconds ago */
    if(utils_ticksdiff(ticks, pheader->lscavenge) < 10000)
    {
        goto Finished;
    }

    pheader->lscavenge = ticks;

    /* Run scavenger starting from start */
    sindex = pheader->scstart;
    eindex = sindex + PER_RUN_SCAVENGE_COUNT;
    pheader->scstart = eindex;

    if(eindex >= pheader->valuecount)
    {
        eindex = pheader->valuecount;
        pheader->scstart = 0;
    }

    dprintimportant("zvcache scavenger sindex = %d, eindex = %d", sindex, eindex);
    for( ;sindex < eindex; sindex++)
    {
        if(pheader->values[sindex] == 0)
        {
            continue;
        }

        pvalue = ZVCACHE_VALUE(palloc, pheader->values[sindex]);
        while(pvalue != NULL)
        {
            ptemp = pvalue;
            pvalue = ZVCACHE_VALUE(palloc, pvalue->next_value);

            /* If ttlive is 0, entry will stay unless deleted */
            if(ptemp->ttlive == 0)
            {
                continue;
            }

            /* Remove the entry from cache if its expired */
            tickdiff = utils_ticksdiff(ticks, ptemp->add_ticks) / 1000;
            if(tickdiff >= ptemp->ttlive)
            {
                remove_zvcache_entry(pcache, sindex, ptemp);
                ptemp = NULL;
            }
        }
    }

Finished:

    dprintverbose("end run_aplist_scavenger");
    return;
}

/* Public functions */
int zvcache_create(zvcache_context ** ppcache)
{
    int               result = NONFATAL;
    zvcache_context * pcache = NULL;

    dprintverbose("start zvcache_create");

    _ASSERT(ppcache != NULL);
    *ppcache = NULL;

    pcache = (zvcache_context *)alloc_pemalloc(sizeof(zvcache_context));
    if(pcache == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pcache->id          = gzvcacheid++;
    pcache->islocal     = 0;
    pcache->cachekey    = 0;
    pcache->hinitdone   = NULL;
    pcache->issession   = 0;

    pcache->incopy      = NULL;
    pcache->outcopy     = NULL;

    pcache->zvmemaddr   = NULL;
    pcache->zvheader    = NULL;
    pcache->zvfilemap   = NULL;
    pcache->zvrwlock    = NULL;
    pcache->zvalloc     = NULL;

    *ppcache = pcache;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_create", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end zvcache_create");

    return result;
}

void zvcache_destroy(zvcache_context * pcache)
{
    dprintverbose("start zvcache_destroy");

    if(pcache != NULL)
    {
        alloc_pefree(pcache);
        pcache = NULL;
    }

    dprintverbose("end zvcache_destroy");

    return;
}

int zvcache_initialize(zvcache_context * pcache, unsigned int issession, unsigned short islocal, unsigned short cachekey, unsigned int zvcount, unsigned int cachesize, char * shmfilepath TSRMLS_DC)
{
    int              result    = NONFATAL;
    size_t           segsize   = 0;
    zvcache_header * header    = NULL;
    unsigned int     msize     = 0;

    unsigned int    cticks     = 0;
    unsigned short  mapclass   = FILEMAP_MAP_SRANDOM;
    unsigned short  locktype   = LOCK_TYPE_SHARED;
    unsigned char   isfirst    = 1;
    unsigned int    initmemory = 0;
    char            evtname[   MAX_PATH];

    dprintverbose("start zvcache_initialize");

    _ASSERT(pcache    != NULL);
    _ASSERT(zvcount   >= 128 && zvcount   <= 1024);
    _ASSERT(cachesize >= 2   && cachesize <= 64);

    pcache->issession = issession;

    /* Initialize memory map to store file list */
    result = filemap_create(&pcache->zvfilemap);
    if(FAILED(result))
    {
        goto Finished;
    }

    pcache->cachekey = cachekey;

    if(islocal)
    {
        mapclass = FILEMAP_MAP_LRANDOM;
        locktype = LOCK_TYPE_LOCAL;

        pcache->islocal = islocal;
    }

    /* If a shmfilepath is passed, use that to create filemap */
    result = filemap_initialize(pcache->zvfilemap, ((issession) ? FILEMAP_TYPE_SESSZVALS : FILEMAP_TYPE_USERZVALS), cachekey, mapclass, cachesize, shmfilepath TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    pcache->zvmemaddr = (char *)pcache->zvfilemap->mapaddr;
    segsize = filemap_getsize(pcache->zvfilemap TSRMLS_CC);
    initmemory = (pcache->zvfilemap->existing == 0);

    /* Create allocator for file list segment */
    result = alloc_create(&pcache->zvalloc);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = alloc_initialize(pcache->zvalloc, islocal, ((issession) ? "SESSZVALS_SEGMENT" : "USERZVALS_SEGMENT"), cachekey, pcache->zvfilemap->mapaddr, segsize, initmemory TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Get memory for cache header */
    msize = sizeof(zvcache_header) + ((zvcount - 1) * sizeof(size_t));
    pcache->zvheader = (zvcache_header *)alloc_get_cacheheader(pcache->zvalloc, msize, ((issession) ? CACHE_TYPE_SESSZVALS : CACHE_TYPE_USERZVALS));
    if(pcache->zvheader == NULL)
    {
        result = FATAL_ZVCACHE_INITIALIZE;
        goto Finished;
    }

    header = pcache->zvheader;

    /* Create reader writer locks for the zvcache */
    result = lock_create(&pcache->zvrwlock);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = lock_initialize(pcache->zvrwlock, ((issession) ? "SESSZVALS_CACHE" : "USERZVALS_CACHE"), cachekey, locktype, LOCK_USET_SREAD_XWRITE, &header->rdcount TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Initialize zvcache_header */
    result = lock_getnewname(pcache->zvrwlock, "ZVCACHE_INIT", evtname, MAX_PATH);
    if(FAILED(result))
    {
        goto Finished;
    }

    pcache->hinitdone = CreateEvent(NULL, TRUE, FALSE, evtname);
    if(pcache->hinitdone == NULL)
    {
        result = FATAL_ZVCACHE_INIT_EVENT;
        goto Finished;
    }

    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        _ASSERT(islocal == 0);
        isfirst = 0;

        /* Wait for other process to initialize completely */
        WaitForSingleObject(pcache->hinitdone, INFINITE);
    }

    /* Initialize the zvcache_header if this is the first process */
    if(islocal || isfirst)
    {
        cticks = GetTickCount();

        if(initmemory)
        {
            /* No need to get a write lock as other processes */
            /* are blocked waiting for hinitdone event */

            /* We can set rdcount to 0 safely as other processes are */
            /* blocked and this process is right now not using lock */
            header->hitcount     = 0;
            header->misscount    = 0;
            header->rdcount      = 0;

            header->lscavenge    = cticks;
            header->scstart      = 0;
            header->itemcount    = 0;
            header->valuecount   = zvcount;

            memset((void *)header->values, 0, sizeof(size_t) * zvcount);
        }

        header->init_ticks = cticks;
        header->mapcount   = 1;

        SetEvent(pcache->hinitdone);
    }
    else
    {
        /* Increment the mapcount */
        InterlockedIncrement(&header->mapcount);
    }

    /* Create incopy and outcopy zvcopy contexts */
    pcache->incopy = (zvcopy_context *)alloc_pemalloc(sizeof(zvcopy_context));
    if(pcache->incopy == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pcache->incopy->oomcode   = FATAL_OUT_OF_SMEMORY;
    pcache->incopy->palloc    = pcache->zvalloc;
    pcache->incopy->pbaseadr  = pcache->zvmemaddr;
    pcache->incopy->hoffset   = 0;
    pcache->incopy->allocsize = 0;
    pcache->incopy->fnmalloc  = alloc_osmalloc;
    pcache->incopy->fnrealloc = alloc_osrealloc;
    pcache->incopy->fnstrdup  = alloc_osstrdup;
    pcache->incopy->fnfree    = alloc_osfree;

    pcache->outcopy = (zvcopy_context *)alloc_pemalloc(sizeof(zvcopy_context));
    if(pcache->outcopy == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pcache->outcopy->oomcode   = FATAL_OUT_OF_LMEMORY;
    pcache->outcopy->palloc    = NULL;
    pcache->outcopy->pbaseadr  = pcache->zvmemaddr;
    pcache->outcopy->hoffset   = 0;
    pcache->outcopy->allocsize = 0;
    pcache->outcopy->fnmalloc  = alloc_oemalloc;
    pcache->outcopy->fnrealloc = alloc_oerealloc;
    pcache->outcopy->fnstrdup  = alloc_oestrdup;
    pcache->outcopy->fnfree    = alloc_oefree;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_initialize", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pcache->zvfilemap != NULL)
        {
            filemap_terminate(pcache->zvfilemap);
            filemap_destroy(pcache->zvfilemap);

            pcache->zvfilemap = NULL;
        }

        if(pcache->zvalloc != NULL)
        {
            alloc_terminate(pcache->zvalloc);
            alloc_destroy(pcache->zvalloc);

            pcache->zvalloc = NULL;
        }

        if(pcache->zvrwlock != NULL)
        {
            lock_terminate(pcache->zvrwlock);
            lock_destroy(pcache->zvrwlock);

            pcache->zvrwlock = NULL;
        }

        if(pcache->hinitdone != NULL)
        {
            CloseHandle(pcache->hinitdone);
            pcache->hinitdone = NULL;
        }

        pcache->zvheader = NULL;
    }

    dprintverbose("end zvcache_initialize");

    return result;
}

void zvcache_terminate(zvcache_context * pcache)
{
    dprintverbose("start zvcache_terminate");

    if(pcache != NULL)
    {
        if(pcache->zvheader != NULL)
        {
            InterlockedDecrement(&pcache->zvheader->mapcount);
            pcache->zvheader = NULL;
        }

        if(pcache->zvalloc != NULL)
        {
            alloc_terminate(pcache->zvalloc);
            alloc_destroy(pcache->zvalloc);

            pcache->zvalloc = NULL;
        }

        if(pcache->zvfilemap != NULL)
        {
            filemap_terminate(pcache->zvfilemap);
            filemap_destroy(pcache->zvfilemap);

            pcache->zvfilemap = NULL;
        }

        if(pcache->zvrwlock != NULL)
        {
            lock_terminate(pcache->zvrwlock);
            lock_destroy(pcache->zvrwlock);

            pcache->zvrwlock = NULL;
        }

        if(pcache->hinitdone != NULL)
        {
            CloseHandle(pcache->hinitdone);
            pcache->hinitdone = NULL;
        }
    }

    dprintverbose("end zvcache_terminate");

    return;
}

int zvcache_get(zvcache_context * pcache, const char * key, zval ** pvalue TSRMLS_DC)
{
    int              result  = NONFATAL;
    unsigned int     index   = 0;
    unsigned char    flock   = 0;
    zvcache_value *  pentry  = NULL;
    zvcache_header * header  = NULL;
    zv_zval *        pcopied = NULL;

    dprintverbose("start zvcache_get");

    _ASSERT(pcache != NULL);
    _ASSERT(key    != NULL);
    _ASSERT(pvalue != NULL);

    header = pcache->zvheader;
    index = utils_getindex(key, pcache->zvheader->valuecount);

    lock_readlock(pcache->zvrwlock);
    flock = 1;

    result = find_zvcache_entry(pcache, key, index, &pentry);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Adjust overall cache hitcount/misscount and entry hitcount */
    if(pentry == NULL)
    {
        InterlockedIncrement(&header->misscount);

        result = WARNING_ZVCACHE_EMISSING;
        goto Finished;
    }
    else
    {
        InterlockedIncrement(&pentry->hitcount);
        InterlockedIncrement(&header->hitcount);
    }

    /* Entry found. copyout to local memory */
    _ASSERT(pentry->zvalue  != 0);
    _ASSERT(pcache->outcopy != NULL);

    InterlockedExchange(&pentry->use_ticks, GetTickCount());
    pcopied = (zv_zval *)ZVALUE(pcache->incopy, pentry->zvalue);

    result = copyout_zval(pcache, pcache->outcopy, NULL, pcopied, pvalue TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(flock)
    {
        lock_readunlock(pcache->zvrwlock);
        flock = 0;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_get", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end zvcache_get");

    return result;
}

int zvcache_set(zvcache_context * pcache, const char * key, zval * pzval, unsigned int ttl, unsigned char isadd TSRMLS_DC)
{
    int             result  = NONFATAL;
    unsigned int    index   = 0;
    unsigned char   flock   = 0;
    zvcache_value * pentry  = NULL;
    zvcache_value * pnewval = NULL;
    zv_zval *       pcopied = NULL;
    char *          pchar   = NULL;

    dprintverbose("start zvcache_set");

    _ASSERT(pcache != NULL);
    _ASSERT(key    != NULL);
    _ASSERT(pzval  != NULL);

    index = utils_getindex(key, pcache->zvheader->valuecount);

    lock_readlock(pcache->zvrwlock);

    result = find_zvcache_entry(pcache, key, index, &pentry);
    if(pentry != NULL && SUCCEEDED(result))
    {
        InterlockedExchange(&pentry->use_ticks, GetTickCount());
    }

    lock_readunlock(pcache->zvrwlock);

    if(FAILED(result))
    {
        goto Finished;
    }

    /* If key already exists, throw error for add */
    if(pentry != NULL && isadd == 1)
    {
        _ASSERT(pentry->zvalue != 0);

        result = WARNING_ZVCACHE_EXISTS;
        goto Finished;
    }

    /* Only update the session entry if the value changed */
    if(pcache->issession)
    {
        _ASSERT((Z_TYPE_P(pzval) & IS_CONSTANT_TYPE_MASK) == IS_STRING);

        if(pentry != NULL)
        {
            pcopied = (zv_zval *)ZVALUE(pcache->incopy, pentry->zvalue);
            _ASSERT((pcopied->type & IS_CONSTANT_TYPE_MASK) == IS_STRING);

            pchar = (char *)ZVALUE(pcache->incopy, pcopied->value.str.val);
            if(Z_STRLEN_P(pzval) == pcopied->value.str.len && strcmp(Z_STRVAL_P(pzval), pchar) == 0)
            {
                /* Shortcircuit set as value stored in existing entry is the same */
                goto Finished;
            }
        }
    }

    /* If entry wasn't found or set was called, create new data */
    result = create_zvcache_data(pcache, key, pzval, ttl, &pnewval TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    lock_writelock(pcache->zvrwlock);
    flock = 1;

    run_zvcache_scavenger(pcache);

    /* Check again after getting the write lock */
    result = find_zvcache_entry(pcache, key, index, &pentry);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* some other process already added this entry, destroy cache data */
    if(pentry != NULL)
    {
        if(isadd == 1)
        {
            destroy_zvcache_data(pcache, pnewval);
            _ASSERT(pentry->zvalue != 0);

            result = WARNING_ZVCACHE_EXISTS;
            goto Finished;
        }
        else
        {
            /* Delete the existing entry */
            remove_zvcache_entry(pcache, index, pentry);
            pentry = NULL;
        }
    }

    pentry = pnewval;
    pnewval = NULL;

    add_zvcache_entry(pcache, index, pentry);
    _ASSERT(pentry != NULL);

Finished:

    if(flock)
    {
        lock_writeunlock(pcache->zvrwlock);
        flock = 0;
    }

    if(pnewval != NULL)
    {
        destroy_zvcache_data(pcache, pnewval);
        pnewval = NULL;
    }
    
    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_set", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end zvcache_set");

    return result;
}

int zvcache_delete(zvcache_context * pcache, const char * key)
{
    int             result = NONFATAL;
    unsigned int    index     = 0;
    unsigned char   flock     = 0;
    zvcache_value * pentry    = NULL;

    dprintverbose("start zvcache_delete");

    _ASSERT(pcache != NULL);
    _ASSERT(key    != NULL);

    index = utils_getindex(key, pcache->zvheader->valuecount);

    lock_writelock(pcache->zvrwlock);
    flock = 1;

    run_zvcache_scavenger(pcache);

    result = find_zvcache_entry(pcache, key, index, &pentry);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(pentry == NULL)
    {
        result = WARNING_ZVCACHE_EMISSING;
        goto Finished;
    }

    remove_zvcache_entry(pcache, index, pentry);

    _ASSERT(SUCCEEDED(result));

Finished:

    if(flock)
    {
        lock_writeunlock(pcache->zvrwlock);
        flock = 0;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_delete", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end zvcache_delete");

    return result;
}

int zvcache_clear(zvcache_context * pcache)
{
    int              result = NONFATAL;
    unsigned int     index  = 0;
    zvcache_header * header = NULL;
    alloc_context *  palloc = NULL;
    zvcache_value *  pvalue = NULL;
    zvcache_value *  ptemp  = NULL;

    dprintverbose("start zvcache_clear");

    _ASSERT(pcache != NULL);

    lock_writelock(pcache->zvrwlock);

    /* No point running the scavenger on call to clear */

    header = pcache->zvheader;
    palloc = pcache->zvalloc;

    for(index = 0; index < header->valuecount; index++)
    {
        if(header->values[index] == 0)
        {
            continue;
        }

        pvalue = ZVCACHE_VALUE(palloc, header->values[index]);

        while(pvalue != NULL)
        {
            ptemp = ZVCACHE_VALUE(palloc, pvalue->next_value);
            remove_zvcache_entry(pcache, index, pvalue);
            pvalue = ptemp;
        }

        ptemp = NULL;
    }

    _ASSERT(SUCCEEDED(result));

    lock_writeunlock(pcache->zvrwlock);

    dprintverbose("end zvcache_clear");

    return result;
}

int zvcache_exists(zvcache_context * pcache, const char * key, unsigned char * pexists)
{
    int             result = NONFATAL;
    unsigned int    index  = 0;
    unsigned char   flock  = 0;
    zvcache_value * pentry = NULL;

    dprintverbose("start zvcache_exists");

    _ASSERT(pcache  != NULL);
    _ASSERT(key     != NULL);
    _ASSERT(pexists != NULL);

    *pexists = 0;
    index = utils_getindex(key, pcache->zvheader->valuecount);

    lock_readlock(pcache->zvrwlock);
    flock = 1;

    result = find_zvcache_entry(pcache, key, index, &pentry);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(pentry == NULL)
    {
        *pexists = 0;
    }
    else
    {
        *pexists = 1;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(flock)
    {
        lock_readunlock(pcache->zvrwlock);
        flock = 0;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_exists", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end zvcache_exists");

    return result;
}

static void zvcache_info_entry_dtor(void * pvoid)
{
    zvcache_info_entry * pinfo = NULL;

    _ASSERT(pvoid != NULL);
    pinfo = (zvcache_info_entry *)pvoid;

    if(pinfo->key != NULL)
    {
        alloc_efree(pinfo->key);
        pinfo->key = NULL;
    }

    return;
}

int zvcache_list(zvcache_context * pcache, zend_bool summaryonly, char * pkey, zvcache_info * pcinfo, zend_llist * plist)
{
    int                  result   = NONFATAL;
    unsigned char        flock    = 0;
    zvcache_header *     header   = NULL;
    alloc_context *      palloc   = NULL;
    zvcache_value *      pvalue   = NULL;
    unsigned int         index    = 0;
    zvcache_info_entry   pinfo    = {0};
    unsigned int         cticks   = 0;
    unsigned int         count    = 0;

    dprintverbose("start zvcache_list");

    _ASSERT(pcache != NULL);
    _ASSERT(pcinfo != NULL);
    _ASSERT(plist  != NULL);

    header = pcache->zvheader;
    palloc = pcache->zvalloc;
    cticks  = GetTickCount();

    lock_readlock(pcache->zvrwlock);
    flock = 1;

    pcinfo->hitcount  = header->hitcount;
    pcinfo->misscount = header->misscount;
    pcinfo->itemcount = header->itemcount;
    pcinfo->islocal   = pcache->islocal;

    zend_llist_init(plist, sizeof(zvcache_info_entry), zvcache_info_entry_dtor, 0);

    pcinfo->initage = utils_ticksdiff(cticks, header->init_ticks) / 1000;

    if(pkey != NULL)
    {
        index = utils_getindex(pkey, header->valuecount);
 
        result = find_zvcache_entry(pcache, pkey, index, &pvalue);
        if(FAILED(result))
        {
            goto Finished;
        }

        if(pvalue != NULL)
        {
            pinfo.key = alloc_estrdup(ZVALUE(pcache->incopy, pvalue->keystr));
            if(pinfo.key == NULL)
            {
                result = FATAL_OUT_OF_LMEMORY;
                goto Finished;
            }

            pinfo.ttl      = pvalue->ttlive;
            pinfo.age      = utils_ticksdiff(cticks, pvalue->add_ticks) / 1000;
            pinfo.type     = (((zv_zval *)ZVALUE(pcache->incopy, pvalue->zvalue))->type) & IS_CONSTANT_TYPE_MASK;
            pinfo.sizeb    = pvalue->sizeb;
            pinfo.hitcount = pvalue->hitcount;

            zend_llist_add_element(plist, &pinfo);
        }
    }
    else
    {
        /* Leave count to 0 if only summary is needed */
        if(!summaryonly)
        {
            count = header->valuecount;
        }

        for(index = 0; index < count; index++)
        {
            if(header->values[index] == 0)
            {
                continue;
            }

            pvalue = ZVCACHE_VALUE(palloc, header->values[index]);
            while(pvalue != NULL)
            {
                pinfo.key = alloc_estrdup(ZVALUE(pcache->incopy, pvalue->keystr));
                if(pinfo.key == NULL)
                {
                    result = FATAL_OUT_OF_LMEMORY;
                    goto Finished;
                }

                pinfo.ttl      = pvalue->ttlive;
                pinfo.age      = utils_ticksdiff(cticks, pvalue->add_ticks) / 1000;
                pinfo.type     = (((zv_zval *)ZVALUE(pcache->incopy, pvalue->zvalue))->type) & IS_CONSTANT_TYPE_MASK;
                pinfo.sizeb    = pvalue->sizeb;
                pinfo.hitcount = pvalue->hitcount;

                zend_llist_add_element(plist, &pinfo);
                pvalue = ZVCACHE_VALUE(palloc, pvalue->next_value);
            }
        }
    }

Finished:

    if(flock)
    {
        lock_readunlock(pcache->zvrwlock);
        flock = 0;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_list", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        zend_llist_destroy(plist);
    }

    dprintverbose("end zvcache_list");

    return result;
}

int zvcache_change(zvcache_context * pcache, const char * key, int delta, int * newvalue)
{
    int             result = NONFATAL;
    unsigned char   flock  = 0;
    unsigned int    index  = 0;
    zvcache_value * pvalue = NULL;
    zv_zval *       pzval  = NULL;

    dprintverbose("start zvcache_change");

    _ASSERT(pcache   != NULL);
    _ASSERT(key      != NULL);
    _ASSERT(newvalue != NULL);

    *newvalue = 0;
    index = utils_getindex(key, pcache->zvheader->valuecount);

    lock_writelock(pcache->zvrwlock);
    flock = 1;

    run_zvcache_scavenger(pcache);

    result = find_zvcache_entry(pcache, key, index, &pvalue);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(pvalue == NULL)
    {
        result = WARNING_ZVCACHE_EMISSING;
        goto Finished;
    }

    _ASSERT(pvalue->zvalue != 0);
    pzval = (zv_zval *)ZVALUE(pcache->incopy, pvalue->zvalue);

    if((pzval->type & IS_CONSTANT_TYPE_MASK) != IS_LONG)
    {
        result = WARNING_ZVCACHE_NOTLONG;
        goto Finished;
    }

    pzval->value.lval += delta;
    *newvalue = pzval->value.lval;

Finished:

    if(flock)
    {
        lock_writeunlock(pcache->zvrwlock);
        flock = 0;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_change", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end zvcache_change");

    return result;
}

int zvcache_compswitch(zvcache_context * pcache, const char * key, int oldvalue, int newvalue)
{
    int             result = NONFATAL;
    unsigned char   flock  = 0;
    unsigned int    index  = 0;
    zvcache_value * pentry = NULL;
    zv_zval *       pzval  = NULL;

    dprintverbose("start zvcache_compswitch");

    _ASSERT(pcache != NULL);
    _ASSERT(key    != NULL);

    index = utils_getindex(key, pcache->zvheader->valuecount);

    lock_writelock(pcache->zvrwlock);
    flock = 1;

    run_zvcache_scavenger(pcache);

    result = find_zvcache_entry(pcache, key, index, &pentry);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(pentry == NULL)
    {
        result = WARNING_ZVCACHE_EMISSING;
        goto Finished;
    }

    _ASSERT(pentry->zvalue != 0);
    pzval = (zv_zval *)ZVALUE(pcache->incopy, pentry->zvalue);

    if((pzval->type & IS_CONSTANT_TYPE_MASK) != IS_LONG)
    {
        result = WARNING_ZVCACHE_NOTLONG;
        goto Finished;
    }

    if(pzval->value.lval == oldvalue)
    {
        pzval->value.lval = newvalue;
    }
    else
    {
	result = WARNING_ZVCACHE_CASNEQ;
	goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(flock)
    {
        lock_writeunlock(pcache->zvrwlock);
        flock = 0;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in zvcache_compswitch", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end zvcache_compswitch");

    return result;
}
