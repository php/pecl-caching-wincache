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
   | Module: wincache_opcopy.c                                                                    |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

#ifndef IS_CONSTANT_TYPE_MASK
#define IS_CONSTANT_TYPE_MASK (~IS_CONSTANT_INDEX)
#endif

#define OMALLOC(popcopy, size)         (popcopy->fnmalloc(popcopy->palloc, popcopy->hoffset, size))
#define OREALLOC(popcopy, addr, size)  (popcopy->fnrealloc(popcopy->palloc, popcopy->hoffset, addr, size))
#define OSTRDUP(popcopy, pstr)         (popcopy->fnstrdup(popcopy->palloc, popcopy->hoffset, pstr))
#define OFREE(popcopy, addr)           (popcopy->fnfree(popcopy->palloc, popcopy->hoffset, addr))

#define DO_FREE  1
#define NO_FREE  0

#define CHECK(p) { if ((p) == NULL){ result = popcopy->oomcode; goto Finished; } }
#define IS_TAILED(container,member) ((char*)(container)+sizeof(*container) == (char*)(container->member))

/* Static function declarations */
static int  copy_zval(opcopy_context * popcopy, zval * poldz, zval ** ppnewz);
static int  copy_zval_ref(opcopy_context * popcopy, zval ** ppoldz, zval *** pppnewz);
#ifdef ZEND_ENGINE_2_4
static int  copy_zval_array(opcopy_context * popcopy, zval ** ppoldz, unsigned int count, zval ***ppnew);
#endif /* ZEND_ENGINE_2_4 */
static int  copy_zend_property_info(opcopy_context * popcopy, zend_property_info * poldp, zend_property_info ** ppnewp);
static int  copy_zend_arg_info(opcopy_context * popcopy, zend_arg_info * poldarg, zend_arg_info ** ppnewarg);
static int  copy_zend_arg_info_array(opcopy_context * popcopy, zend_arg_info * pold, unsigned int count, zend_arg_info ** ppnew);
#ifdef ZEND_ENGINE_2_4
static int  copy_znode_op(opcopy_context * popcopy, int op_type, znode_op * poldz, znode_op ** ppnewz);
#else /* ZEND_ENGINE_2_3 and below */
static int  copy_znode(opcopy_context * popcopy, znode * poldz, znode ** ppnewz);
#endif /* ZEND_ENGINE_2_4 */
static int  copy_zend_op(opcopy_context * popcopy, zend_op * poldop, zend_op ** ppnewop);
static int  copy_zend_op_array(opcopy_context * popcopy, zend_op_array * poparray, zend_op_array ** ppoparray);
static int  copy_zend_function(opcopy_context * popcopy, zend_function * poldf, zend_function ** ppnew);
static int  copy_zend_function_entry(opcopy_context * popcopy, zend_function_entry * poldfe, zend_function_entry ** ppnewfe);
static int  copy_hashtable(opcopy_context * popcopy, HashTable * poldh, unsigned int copy_flag, HashTable ** ppnewh, void * parg1, void * parg2);
static int  check_hashtable_bucket(Bucket * pbucket, unsigned int copy_flag, void * parg1, void * parg2);
static int  copy_hashtable_bucket(opcopy_context * popcopy, Bucket * poldb, unsigned int copy_flag, Bucket ** ppnewb);
static int  copy_zend_class_entry(opcopy_context * popcopy, zend_class_entry * poldce, zend_class_entry ** ppnewce);
static int  copy_zend_constant_entry(opcopy_context * popcopy, zend_constant * poldt, zend_constant ** ppnewt);

static void free_zval(opcopy_context * popcopy, zval * pvalue, unsigned char ffree);
static void free_zval_ref(opcopy_context * popcopy, zval ** ppvalue, unsigned char ffree);
#ifdef ZEND_ENGINE_2_4
static void free_zval_array(opcopy_context * popcopy, zval ** ppvalues, unsigned int count, unsigned char ffree);
#endif /* ZEND_ENGINE_2_4 */
static void free_zend_property_info(opcopy_context * popcopy, zend_property_info * pvalue, unsigned char ffree);
static void free_zend_arg_info(opcopy_context * popcopy, zend_arg_info * pvalue, unsigned char ffree);
static void free_zend_arg_info_array(opcopy_context * popcopy, zend_arg_info * pvalue, unsigned int count, unsigned char ffree);
#ifdef ZEND_ENGINE_2_4
static void free_znode_op(opcopy_context * popcopy, int op_type, znode_op * pvalue, unsigned char ffree);
#else /* ZEND_ENGINE_2_3 and below */
static void free_znode(opcopy_context * popcopy, znode * pvalue, unsigned char ffree);
#endif /* ZEND_ENGINE_2_4 */
static void free_zend_op(opcopy_context * popcopy, zend_op * pvalue, unsigned char ffree);
static void free_zend_op_array(opcopy_context * popcopy, zend_op_array * pvalue, unsigned char ffree);
static void free_zend_function(opcopy_context * popcopy, zend_function * pvalue, unsigned char ffree);
static void free_zend_function_entry(opcopy_context * popcopy, zend_function_entry * pvalue, unsigned char ffree);
static void free_hashtable(opcopy_context * popcopy, HashTable * pvalue, unsigned char ffree);
static void free_hashtable_bucket(opcopy_context * popcopy, Bucket * pvalue, unsigned char ffree);
static void free_zend_class_entry(opcopy_context * popcopy, zend_class_entry * pvalue, unsigned char ffree);

static int  copyin_zend_op_array(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);
static int  copyin_zend_functions(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);
static int  copyin_zend_classes(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);
static int  copyin_zend_constants(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);
static int  copyin_messages(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);
static int  copyin_auto_globals(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);

static int  copyout_zend_op_array(opcopy_context * popcopy, zend_op_array * poldopa, zend_op_array ** ppnewopa);
static int  copyout_zend_function(opcopy_context * popcopy, zend_function * poldf, zend_function ** ppnewf);
static int  copyout_zend_class(opcopy_context * popcopy, zend_class_entry * poldce, zend_class_entry ** ppnewce);
static int  copyout_zend_constant(opcopy_context * popcopy, zend_constant * poldt, zend_constant ** pnewt);

static int  wincache_string_pmemcopy(opcopy_context * popcopy, const char *str, size_t len, const char **newStr);

#ifdef ZEND_ENGINE_2_4
static zend_trait_alias* opcopy_trait_alias(opcopy_context * popcopy, zend_trait_alias *src);
static zend_trait_precedence* opcopy_trait_precedence(opcopy_context * popcopy, zend_trait_precedence *src);
#endif /* ZEND_ENGINE_2_4 */

#ifdef ZEND_ENGINE_2_6
static void free_zend_ast(opcopy_context * popcopy, zend_ast *ast, unsigned char ffree);
static int copy_zend_ast(opcopy_context * popcopy, zend_ast *ast, zend_ast **new_ast TSRMLS_DC);
#endif /* ZEND_ENGINE_2_6 */


/* copyin functions are to copy internal data structures to shared memory */
/* Use offsets where ever pointers types are used */

/* copyout functions are to copy internal data structures from shared */
/* memory to process local memory take care of offsets */

static int wincache_string_pmemcopy(
    opcopy_context * popcopy,
    const char *str,
    size_t len,
    const char **newStr
    )
{
    int result = NONFATAL;
    char * ret = NULL;

    TSRMLS_FETCH();

    *newStr = NULL;

#ifdef ZEND_ENGINE_2_4
#ifndef ZTS
    if (popcopy->optype == OPCOPY_OPERATION_COPYOUT) {
        ret = (char*)wincache_new_interned_string((const char*)str, len TSRMLS_CC);
        if (ret) {
            *newStr = ret;
            goto Finished;
        }
    }
#endif /* !ZTS */
#endif /* ZEND_ENGINE_2_4 */
    ret = OMALLOC(popcopy, len);
    if(ret == NULL)
    {
        result = popcopy->oomcode;
        goto Finished;
    }

    memcpy_s(ret, len, str, len);
    *newStr = ret;

Finished:
    return result;
}

#ifdef ZEND_ENGINE_2_6
static int copy_zend_ast(opcopy_context * popcopy, zend_ast *ast, zend_ast **new_ast TSRMLS_DC)
{
    int     result = NONFATAL;
    int     i;
    zend_ast *node;

    if (ast->kind == ZEND_CONST)
    {
        node = (zend_ast *)OMALLOC(popcopy, sizeof(zend_ast) + sizeof(zval));
        if(node == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        node->kind = ZEND_CONST;
        node->children = 0;
        node->u.val = (zval*)(node + 1);
        *node->u.val = *ast->u.val;
        if ((Z_TYPE_P(ast->u.val) & IS_CONSTANT_TYPE_MASK) >= IS_ARRAY) {
            switch ((Z_TYPE_P(ast->u.val) & IS_CONSTANT_TYPE_MASK)) {
                case IS_STRING:
                case IS_CONSTANT:
                    result = wincache_string_pmemcopy(popcopy, Z_STRVAL_P(ast->u.val), Z_STRLEN_P(ast->u.val) + 1, &Z_STRVAL_P(node->u.val));
                    if(FAILED(result))
                    {
                        goto Finished;
                    }
                    break;
                case IS_ARRAY:
#if !defined(ZEND_ENGINE_2_6)
                case IS_CONSTANT_ARRAY:
#endif
                    if (ast->u.val->value.ht && ast->u.val->value.ht != &EG(symbol_table)) {
                        /* Copy zval pointers in the hashtable */
                        node->u.val->value.ht = NULL;
                        dprintverbose("copy_zend_ast calling copy_hashtable");
                        result = copy_hashtable(popcopy,
                                                Z_ARRVAL_P(ast->u.val),
                                                copy_flag_zval_ref | copy_flag_pDataPtr,
                                                &(node->u.val->value.ht),
                                                NULL,
                                                NULL);
                        if(FAILED(result))
                        {
                            goto Finished;
                        }
                    }
                    break;
                case IS_CONSTANT_AST:
                    result = copy_zend_ast(popcopy, Z_AST_P(ast->u.val), &Z_AST_P(node->u.val) TSRMLS_CC);
                    break;
            }
        }
    } else {
        node = (zend_ast *)OMALLOC(popcopy, sizeof(zend_ast) + (sizeof(zend_ast*) * (ast->children)));
        if(node == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        node->kind = ast->kind;
        node->children = ast->children;
        for (i = 0; i < ast->children; i++) {
            if ((&ast->u.child)[i]) {
                result = copy_zend_ast(popcopy, (&ast->u.child)[i], &(&node->u.child)[i] TSRMLS_CC);
                if (FAILED(result))
                {
                    goto Finished;
                }
            } else {
                (&node->u.child)[i] = NULL;
            }
        }
    }

    *new_ast = node;

Finished:
    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zval", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if (node)
        {
            free_zend_ast(popcopy, node, TRUE);
            node = NULL;
        }
    }

    return result;
}

static void free_zend_ast(opcopy_context * popcopy, zend_ast *ast, unsigned char ffree)
{
    int i;

    if (!ast)
    {
        return;
    }

    if (ast->kind == ZEND_CONST)
    {
        if ((Z_TYPE_P(ast->u.val) & IS_CONSTANT_TYPE_MASK) >= IS_ARRAY) {
            switch ((Z_TYPE_P(ast->u.val) & IS_CONSTANT_TYPE_MASK)) {
                case IS_STRING:
                case IS_CONSTANT:
                    if (Z_STRVAL_P(ast->u.val) && !IS_INTERNED(Z_STRVAL_P(ast->u.val)))
                    {
                        OFREE(popcopy, Z_STRVAL_P(ast->u.val));
                        Z_STRVAL_P(ast->u.val) = NULL;
                    }
                    break;
                case IS_ARRAY:
#if !defined(ZEND_ENGINE_2_6)
                case IS_CONSTANT_ARRAY:
#endif
                    if (Z_ARRVAL_P(ast->u.val) && Z_ARRVAL_P(ast->u.val) != &EG(symbol_table)) {
                        /* Copy zval pointers in the hashtable */
                        free_hashtable(popcopy, Z_ARRVAL_P(ast->u.val), DO_FREE);
                        Z_ARRVAL_P(ast->u.val) = NULL;
                    }
                    break;
                case IS_CONSTANT_AST:
                    free_zend_ast(popcopy, Z_AST_P(ast->u.val), DO_FREE);
                    break;
            }
        }
    }
    else
    {
        for (i = 0; i < ast->children; i++) {
            if ((&ast->u.child)[i]) {
                free_zend_ast(popcopy, (&ast->u.child)[i], DO_FREE);
            }
        }
    }

}
#endif /* ZEND_ENGINE_2_6 */

static int copy_zval(opcopy_context * popcopy, zval * poldz, zval ** ppnewz)
{
    int     result    = NONFATAL;
    int     allocated = 0;
    int     length    = 0;
    zval *  pnewz     = NULL;

    TSRMLS_FETCH();
    dprintdecorate("start copy_zval");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldz   != NULL);
    _ASSERT(ppnewz  != NULL);

    /* Allocate memory if required */
    if(*ppnewz == NULL)
    {
        if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
        {
            pnewz = (zval *)OMALLOC(popcopy, sizeof(zval));
            if(pnewz == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            allocated = 1;
        }
        else if(popcopy->optype == OPCOPY_OPERATION_COPYOUT)
        {
            ALLOC_ZVAL(pnewz);
            if(pnewz == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            allocated = 2;
        }
    }
    else
    {
        pnewz = *ppnewz;
    }

    _ASSERT(pnewz != NULL);
    memcpy_s(pnewz, sizeof(zval), poldz, sizeof(zval));

    /* TBD?? If poldz->refcount__gc is greater than 0 */
    /* We need to find existing zval and increment its refcount */

    /*pnewz->refcount__gc = poldz->refcount__gc; */
    /*pnewz->is_ref__gc   = poldz->is_ref__gc; */

    switch(poldz->type & IS_CONSTANT_TYPE_MASK)
    {
        case IS_NULL:
        case IS_LONG:
        case IS_DOUBLE:
        case IS_BOOL:
        case IS_RESOURCE:
            /* Bitwise copy is enough. Nothing to do */
            break;

        case IS_STRING:
        case IS_CONSTANT:
            /* Copy string to shared memory */
            if(poldz->value.str.val)
            {
                result = wincache_string_pmemcopy(popcopy, poldz->value.str.val, poldz->value.str.len + 1, &pnewz->value.str.val);
                if(FAILED(result))
                {
                    goto Finished;
                }
            }

            pnewz->value.str.len = poldz->value.str.len;
            break;

        case IS_ARRAY:
#if !defined(ZEND_ENGINE_2_6)
        case IS_CONSTANT_ARRAY:
#endif
            /* Copy zval pointers in the hashtable */
            pnewz->value.ht = NULL;
            dprintverbose("copy_zval calling copy_hashtable");
            result = copy_hashtable(popcopy, poldz->value.ht, copy_flag_zval_ref | copy_flag_pDataPtr, &pnewz->value.ht, NULL, NULL);
            if(FAILED(result))
            {
                goto Finished;
            }
            break;

#ifdef ZEND_ENGINE_2_6
        case IS_CONSTANT_AST:
            result = copy_zend_ast(popcopy, Z_AST_P(poldz), &Z_AST_P(pnewz) TSRMLS_CC);
            if (FAILED(result))
            {
                goto Finished;
            }
            break;
#endif /* ZEND_ENGINE_2_6 */

        case IS_OBJECT:
            /* Copying opcodes is immediately after compile file */
            /* No objects are yet created. Just set to NULL */
            pnewz->type = IS_NULL;
            dprintimportant("ZVAL IS_OBJECT: setting to NULL");
            break;

        default:

            dprintimportant("ZVAL type = %d", poldz->type);
            _ASSERT(FALSE);
            break;
    }

    *ppnewz = pnewz;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zval", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewz != NULL)
        {
            if(allocated == 1)
            {
                OFREE(popcopy, pnewz);
                pnewz = NULL;
            }
            else if(allocated == 2)
            {
                FREE_ZVAL(pnewz);
                pnewz = NULL;
            }
        }
    }

    dprintdecorate("end copy_zval");

    return result;
}

static void free_zval(opcopy_context * popcopy, zval * pvalue, unsigned char ffree)
{
    TSRMLS_FETCH();
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(popcopy->optype == OPCOPY_OPERATION_COPYOUT)
        {
            if(ffree == DO_FREE)
            {
                FREE_ZVAL(pvalue);
                pvalue = NULL;
            }
        }
        else
        {
            switch(pvalue->type & IS_CONSTANT_TYPE_MASK)
            {
                case IS_NULL:
                case IS_LONG:
                case IS_DOUBLE:
                case IS_BOOL:
                case IS_RESOURCE:
                case IS_OBJECT:
                    break;

                case IS_STRING:
                case IS_CONSTANT:
                    if(pvalue->value.str.val
#ifdef ZEND_ENGINE_2_4
                       && !IS_INTERNED(pvalue->value.str.val)
#endif /* ZEND_ENGINE_2_4 */
                       )
                    {
                        OFREE(popcopy, pvalue->value.str.val);
                        pvalue->value.str.val = NULL;
                    }
                    break;

                case IS_ARRAY:
#if !defined(ZEND_ENGINE_2_6)
                case IS_CONSTANT_ARRAY:
#endif
                    if(pvalue->value.ht)
                    {
                        free_hashtable(popcopy, pvalue->value.ht, DO_FREE); /* copy_flag_zval_ref | copy_flag_pDataPtr */
                        pvalue->value.ht = NULL;
                    }
                    break;

#ifdef ZEND_ENGINE_2_6
                case IS_CONSTANT_AST:
                    if (Z_AST_P(pvalue))
                    {
                        free_zend_ast(popcopy, Z_AST_P(pvalue), DO_FREE);
                        Z_AST_P(pvalue) = NULL;
                    }
                    break;
#endif /* ZEND_ENGINE_2_6 */
            }

            if(ffree == DO_FREE)
            {
                OFREE(popcopy, pvalue);
                pvalue = NULL;
            }
        }
    }

    return;
}

static int copy_zval_ref(opcopy_context * popcopy, zval ** ppoldz, zval *** pppnewz)
{
    int     result    = NONFATAL;
    int     allocated = 0;
    zval ** ppnewz    = NULL;

    dprintdecorate("start copy_zval_ref");

    _ASSERT(popcopy != NULL);
    _ASSERT(ppoldz  != NULL);
    _ASSERT(pppnewz != NULL);

    if(*pppnewz == NULL)
    {
        ppnewz = (zval **)OMALLOC(popcopy, sizeof(zval *));
        if(ppnewz == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        ppnewz = *pppnewz;
    }

    *ppnewz = NULL;
    result = copy_zval(popcopy, *ppoldz, ppnewz);
    if(FAILED(result))
    {
        goto Finished;
    }

    *pppnewz = ppnewz;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zval_ref", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(ppnewz != NULL)
        {
            if(allocated != 0)
            {
                OFREE(popcopy, ppnewz);
                ppnewz = NULL;
            }
        }
    }

    dprintdecorate("end copy_zval_ref");

    return result;
}

static void free_zval_ref(opcopy_context * popcopy, zval ** ppvalue, unsigned char ffree)
{
    if(ppvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(*ppvalue != NULL)
        {
            free_zval(popcopy, *ppvalue, DO_FREE);
            *ppvalue = NULL;
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, ppvalue);
            ppvalue = NULL;
        }
    }

    return;
}

#ifdef ZEND_ENGINE_2_4
static int copy_zval_array(opcopy_context * popcopy, zval ** ppoldz, unsigned int count, zval ***ppnew)
{
    int     result     = NONFATAL;
    int     allocated  = 0;
    unsigned int index = 0;
    size_t  size       = 0;
    zval ** prgnewz    = NULL;

    dprintdecorate("start copy_zval_array");

    _ASSERT(popcopy != NULL);
    _ASSERT(ppoldz  != NULL);
    _ASSERT(ppnew  != NULL);
    _ASSERT(count   != 0);

    size = sizeof(zval*) * count;

    if (*ppnew == NULL)
    {
        /* Alloc the array */
        prgnewz = (zval **)OMALLOC(popcopy, size);
        if(prgnewz == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        /* Zero out the memory in the array */
        memset(prgnewz, 0, size);

        allocated = 1;
    }
    else
    {
        prgnewz = *ppnew;
    }

    _ASSERT(prgnewz != NULL);

    /* Copy each zval over from the source to the dest */
    for (index = 0; index < count; index++)
    {
        if (ppoldz[index] != NULL)
        {
            result = copy_zval(popcopy, ppoldz[index], &prgnewz[index]);
            if (FAILED(result))
            {
                goto Finished;
            }
        }
        else
        {
            prgnewz[index] = NULL;
        }
    }

    *ppnew = prgnewz;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zval_array", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(prgnewz != NULL)
        {
            /* First, if we allocated any zval's in the array, free them first */
            for (index = 0; index < count; index++)
            {
                if (prgnewz[index])
                {
                    free_zval(popcopy, prgnewz[index], DO_FREE);
                }
            }

            /* Finally, free the array itself */
            if(allocated == 1)
            {
                OFREE(popcopy, prgnewz);
                prgnewz = NULL;
            }
        }
    }

    dprintdecorate("end copy_zval_array");

    return result;
}

static void free_zval_array(opcopy_context * popcopy, zval ** ppvalues, unsigned int count, unsigned char ffree)
{
    _ASSERT(popcopy  != NULL);
    _ASSERT(ppvalues != NULL);

    if (ppvalues)
    {
        unsigned int index = 0;

        /* First, free all the zval's in the array */
        for (index = 0; index < count; index++)
        {
            free_zval(popcopy, ppvalues[index], ffree);
        }

        /* Then free the array itself */
        if(ffree == DO_FREE)
        {
            OFREE(popcopy, ppvalues);
            ppvalues = NULL;
        }
    }
    return;
}
#endif /* ZEND_ENGINE_2_4 */

static int copy_zend_property_info(opcopy_context * popcopy, zend_property_info * poldp, zend_property_info ** ppnewp)
{
    int                  result    = NONFATAL;
    int                  allocated = 0;
    zend_property_info * pnewp     = NULL;
    unsigned int         namelen   = 0;
    unsigned int         doclen    = 0;

    dprintdecorate("start copy_zend_property_info");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldp   != NULL);
    _ASSERT(ppnewp  != NULL);

    /* Allocate memory if neccessary */
    if(*ppnewp == NULL)
    {
        pnewp = (zend_property_info *)OMALLOC(popcopy, sizeof(zend_property_info));
        if(pnewp == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewp = *ppnewp;
    }

    /* Do a blind copy before doing deep copy of name and doc_comment */
    memcpy_s(pnewp, sizeof(zend_property_info), poldp, sizeof(zend_property_info));

    pnewp->name        = NULL;

    /* pnewp->ce will be set during fixup */
    if(poldp->name != NULL)
    {
        result = wincache_string_pmemcopy(popcopy, poldp->name, poldp->name_length + 1, &pnewp->name);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    /* Do a deep copy for copyin. For copyout, shallow copy is enough */
    if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
    {
        pnewp->doc_comment = NULL;
        if(poldp->doc_comment != NULL)
        {
            doclen =poldp->doc_comment_len + 1;
            pnewp->doc_comment = OMALLOC(popcopy, doclen);
            if(pnewp->doc_comment == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            memcpy_s((void *)pnewp->doc_comment, doclen, poldp->doc_comment, doclen);
        }
    }

    *ppnewp = pnewp;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zend_property_info", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewp != NULL)
        {
            if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
            {
                if(pnewp->name != NULL
#ifdef ZEND_ENGINE_2_4
                   && !IS_INTERNED(pnewp->name)
#endif /* ZEND_ENGINE_2_4 */
                   )
                {
                    OFREE(popcopy, (void *)pnewp->name);
                    pnewp->name = NULL;
                }

                if(pnewp->doc_comment != NULL)
                {
                    OFREE(popcopy, (void *)pnewp->doc_comment);
                    pnewp->doc_comment = NULL;
                }
            }

            if(allocated != 0)
            {
                OFREE(popcopy, pnewp);
                pnewp = NULL;
            }
        }
    }

    dprintdecorate("end copy_zend_property_info");

    return result;
}

static void free_zend_property_info(opcopy_context * popcopy, zend_property_info * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);

        if(pvalue->name != NULL
#ifdef ZEND_ENGINE_2_4
           && !IS_INTERNED(pvalue->name)
#endif /* ZEND_ENGINE_2_4 */
           )
        {
            OFREE(popcopy, (void *)pvalue->name);
            pvalue->name = NULL;
        }

        if(pvalue->doc_comment != NULL)
        {
            OFREE(popcopy, (void *)pvalue->doc_comment);
            pvalue->doc_comment = NULL;
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

static int copy_zend_arg_info(opcopy_context * popcopy, zend_arg_info * poldarg, zend_arg_info ** ppnewarg)
{
    int             result    = NONFATAL;
    int             allocated = 0;
    zend_arg_info * pnewarg   = NULL;
    unsigned int    namelen   = 0;
    unsigned int    cnamelen  = 0;

    dprintdecorate("start copy_zend_arg_info");

    _ASSERT(popcopy  != NULL);
    _ASSERT(poldarg  != NULL);
    _ASSERT(ppnewarg != NULL);

    /* Allocate memory if necessary */
    if(*ppnewarg == NULL)
    {
        pnewarg = (zend_arg_info *)OMALLOC(popcopy, sizeof(zend_arg_info));
        if(pnewarg == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewarg = *ppnewarg;
    }

    memcpy_s(pnewarg, sizeof(zend_arg_info), poldarg, sizeof(zend_arg_info));

    pnewarg->name       = NULL;
    pnewarg->class_name = NULL;

    if(poldarg->name != NULL)
    {
        result = wincache_string_pmemcopy(popcopy, poldarg->name, poldarg->name_len + 1, &pnewarg->name);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    if(poldarg->class_name != NULL)
    {
        result = wincache_string_pmemcopy(popcopy, poldarg->class_name, poldarg->class_name_len + 1, &pnewarg->class_name);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    *ppnewarg = pnewarg;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zend_arg_info", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewarg != NULL)
        {
            if(pnewarg->name != NULL
#ifdef ZEND_ENGINE_2_4
               && !IS_INTERNED(pnewarg->name)
#endif /* ZEND_ENGINE_2_4 */
               )
            {
                OFREE(popcopy, (void *)pnewarg->name);
                pnewarg->name = NULL;
            }

            if(pnewarg->class_name != NULL
#ifdef ZEND_ENGINE_2_4
               && !IS_INTERNED(pnewarg->class_name)
#endif /* ZEND_ENGINE_2_4 */
               )
            {
                OFREE(popcopy, (void *)pnewarg->class_name);
                pnewarg->class_name = NULL;
            }

            if(allocated != 0)
            {
                OFREE(popcopy, pnewarg);
                pnewarg = NULL;
            }
        }
    }

    dprintdecorate("end copy_zend_arg_info");

    return result;
}

static void free_zend_arg_info(opcopy_context * popcopy, zend_arg_info * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);

        if(pvalue->name != NULL
#ifdef ZEND_ENGINE_2_4
           && !IS_INTERNED(pvalue->name)
#endif /* ZEND_ENGINE_2_4 */
           )
        {
            OFREE(popcopy, (void *)pvalue->name);
            pvalue->name = NULL;
        }

        if(pvalue->class_name != NULL
#ifdef ZEND_ENGINE_2_4
           && !IS_INTERNED(pvalue->class_name)
#endif /* ZEND_ENGINE_2_4 */
           )
        {
            OFREE(popcopy, (void *)pvalue->class_name);
            pvalue->class_name = NULL;
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

static int copy_zend_arg_info_array(opcopy_context * popcopy, zend_arg_info * pold, unsigned int count, zend_arg_info ** ppnew)
{
    int             result    = NONFATAL;
    int             allocated = 0;
    unsigned int    index     = 0;
    zend_arg_info * pnew      = NULL;
    zend_arg_info * ptemp     = NULL;

    dprintdecorate("start copy_zend_arg_info_array");

    _ASSERT(popcopy != NULL);
    _ASSERT(pold    != NULL);
    _ASSERT(count   >  0);
    _ASSERT(ppnew   != NULL);

    /* Allocate memory if required */
    if(*ppnew == NULL)
    {
        pnew = (zend_arg_info *)OMALLOC(popcopy, sizeof(zend_arg_info) * count);
        if(pnew == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnew = *ppnew;
    }

    /* Copy zend_arg_info elements one by one */
    for(index = 0; index < count; index++)
    {
        ptemp = &pnew[index];
        result = copy_zend_arg_info(popcopy, &pold[index], &ptemp);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    *ppnew = pnew;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(result != NONFATAL)
    {
        dprintimportant("failure %d in copy_zend_arg_info_array", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnew != NULL)
        {
            if(index > 0)
            {
                count = index;
                for(index = 0; index < count; index++)
                {
                    free_zend_arg_info(popcopy, &pnew[index], NO_FREE);
                }
            }

            if(allocated != 0)
            {
                OFREE(popcopy, pnew);
                pnew = NULL;
            }
        }
    }

    dprintdecorate("end copy_zend_arg_info_array");

    return result;
}

static void free_zend_arg_info_array(opcopy_context * popcopy, zend_arg_info * pvalue, unsigned int count, unsigned char ffree)
{
    unsigned int index = 0;

    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        for(index = 0; index < count; index++)
        {
            free_zend_arg_info(popcopy, &pvalue[index], NO_FREE);
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

#ifdef ZEND_ENGINE_2_3
static int copy_znode(opcopy_context * popcopy, znode * poldz, znode ** ppnewz)
{
    int        result    = NONFATAL;
    int        allocated = 0;
    znode * pnewz     = NULL;
    zval *     ptemp     = NULL;

    dprintdecorate("start copy_znode");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldz   != NULL);
    _ASSERT(ppnewz  != NULL);

    /* Allocate memory if not allocated */
    if(*ppnewz == NULL)
    {
        pnewz = (znode *)OMALLOC(popcopy, sizeof(znode));
        if(pnewz == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewz = *ppnewz;
    }

    /* TBD?? How to make sure op_array pointer and */
    /* jmp_addr pointer are correct in all the processes */
    memcpy_s(pnewz, sizeof(znode), poldz, sizeof(znode));

    if(poldz->op_type == IS_CONST)
    {
        ptemp = &pnewz->u.constant;
        result = copy_zval(popcopy, &poldz->u.constant, &ptemp);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    *ppnewz = pnewz;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_znode", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewz != NULL)
        {
            if(allocated != 0)
            {
                OFREE(popcopy, pnewz);
                pnewz = NULL;
            }
        }
    }

    dprintdecorate("end copy_znode");

    return result;
}

static void free_znode(opcopy_context * popcopy, znode * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(pvalue->op_type == IS_CONST)
        {
            free_zval(popcopy, &pvalue->u.constant, NO_FREE);
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}
#endif /* ZEND_ENGINE_2_3 */

#ifdef ZEND_ENGINE_2_4
static int copy_znode_op(opcopy_context * popcopy, int op_type, znode_op * poldz, znode_op ** ppnewz)
{
    int        result    = NONFATAL;
    int        allocated = 0;
    znode_op * pnewz     = NULL;
    zval *     ptemp     = NULL;

    dprintdecorate("start copy_znode_op");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldz   != NULL);
    _ASSERT(ppnewz  != NULL);

    /* Allocate memory if not allocated */
    if(*ppnewz == NULL)
    {
        pnewz = (znode_op *)OMALLOC(popcopy, sizeof(znode_op));
        if(pnewz == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewz = *ppnewz;
    }

    /* TBD?? How to make sure op_array pointer and */
    /* jmp_addr pointer are correct in all the processes */
    memcpy_s(pnewz, sizeof(znode_op), poldz, sizeof(znode_op));

    if(op_type == IS_CONST)
    {
        pnewz->zv = NULL;
        result = copy_zval(popcopy, poldz->zv, &(pnewz->zv));
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    *ppnewz = pnewz;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_znode", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewz != NULL)
        {
            if(allocated != 0)
            {
                OFREE(popcopy, pnewz);
                pnewz = NULL;
            }
        }
    }

    dprintdecorate("end copy_znode");

    return result;
}

static void free_znode_op(opcopy_context * popcopy, int op_type, znode_op * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(op_type == IS_CONST)
        {
            free_zval(popcopy, pvalue->zv, NO_FREE);
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}
#endif /* ZEND_ENGINE_2_4 */

static int copy_zend_op(opcopy_context * popcopy, zend_op * poldop, zend_op ** ppnewop)
{
    int             result    = NONFATAL;
    int             allocated = 0;
    zend_op *       pnewop    = NULL;
    zend_op *       pnextop   = NULL;
#ifdef ZEND_ENGINE_2_4
    znode_op *      pznode    = NULL;
#else /* ZEND_ENGINE_2_3 and below */
    znode *         pznode    = NULL;
#endif /* ZEND_ENGINE_2_4 */
    char *          frname    = NULL;
    unsigned int    frnlen    = 0;

    TSRMLS_FETCH();
    dprintdecorate("start copy_zend_op");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldop  != NULL);
    _ASSERT(ppnewop != NULL);

    /* Allocate memory if necessary */
    if(*ppnewop == NULL)
    {
        pnewop = (zend_op *)OMALLOC(popcopy, sizeof(zend_op));
        if(pnewop == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewop = *ppnewop;
    }

    memcpy_s(pnewop, sizeof(zend_op), poldop, sizeof(zend_op));

    pznode = &pnewop->result;
#ifdef ZEND_ENGINE_2_4
    result = copy_znode_op(popcopy, poldop->result_type, &poldop->result, &pznode);
#else /* ZEND_ENGINE_2_3 and below */
    result = copy_znode(popcopy, &poldop->result, &pznode);
#endif /* ZEND_ENGINE_2_4 */
    if(FAILED(result))
    {
        goto Finished;
    }

    pznode = &pnewop->op1;
#ifdef ZEND_ENGINE_2_4
    result = copy_znode_op(popcopy, poldop->op1_type, &poldop->op1, &pznode);
#else /* ZEND_ENGINE_2_3 and below */
    result = copy_znode(popcopy, &poldop->op1, &pznode);
#endif /* ZEND_ENGINE_2_4 */
    if(FAILED(result))
    {
        goto Finished;
    }

    pznode = &pnewop->op2;
#ifdef ZEND_ENGINE_2_4
    result = copy_znode_op(popcopy, poldop->op2_type, &poldop->op2, &pznode);
#else /* ZEND_ENGINE_2_3 and below */
    result = copy_znode(popcopy, &poldop->op2, &pznode);
#endif /* ZEND_ENGINE_2_4 */
    if(FAILED(result))
    {
        goto Finished;
    }

    *ppnewop = pnewop;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zend_op", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewop != NULL)
        {
            if(allocated != 0)
            {
                OFREE(popcopy, pnewop);
                pnewop = NULL;
            }
        }
    }

    dprintdecorate("end copy_zend_op");

    return result;
}

static void free_zend_op(opcopy_context * popcopy, zend_op * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);

#ifdef ZEND_ENGINE_2_4
        free_znode_op(popcopy, pvalue->result_type, &pvalue->result, NO_FREE);
        free_znode_op(popcopy, pvalue->op1_type, &pvalue->op1, NO_FREE);
        free_znode_op(popcopy, pvalue->op2_type, &pvalue->op2, NO_FREE);
#else /* ZEND_ENGINE_2_3 and below */
        free_znode(popcopy, &pvalue->result, NO_FREE);
        free_znode(popcopy, &pvalue->op1, NO_FREE);
        free_znode(popcopy, &pvalue->op2, NO_FREE);
#endif /* ZEND_ENGINE_2_4 */

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

static int copy_zend_op_array(opcopy_context * popcopy, zend_op_array * poldopa, zend_op_array ** ppnewopa)
{
    int             result    = NONFATAL;
    int             opcount   = 0;
    int             index     = 0;
    unsigned int    msize     = 0;
    int             bMustFixup = 0;

    zend_op *       pnewop    = NULL;
    zend_op *       poldop    = NULL;
    zend_op_array * pnewopa   = NULL;
    unsigned int *  pdata     = NULL;

    dprintdecorate("start copy_zend_op_array");

    _ASSERT(popcopy   != NULL);
    _ASSERT(poldopa   != NULL);
    _ASSERT(ppnewopa  != NULL);

    if(*ppnewopa == NULL)
    {
        pnewopa = (zend_op_array *)OMALLOC(popcopy, sizeof(zend_op_array));
        if(pnewopa == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
    }
    else
    {
        pnewopa = *ppnewopa;
    }

    if(popcopy->optype == OPCOPY_OPERATION_COPYOUT)
    {
        pdata = (unsigned int *)&(poldopa->reserved[popcopy->resnumber]);
    }
    else
    {
        _ASSERT(popcopy->optype == OPCOPY_OPERATION_COPYIN);
        pdata = (unsigned int *)&(pnewopa->reserved[popcopy->resnumber]);

        *pdata = 0;
        pnewopa->opcodes = NULL;
    }

    memcpy_s(pnewopa, sizeof(zend_op_array), poldopa, sizeof(zend_op_array));

    pnewopa->refcount         = NULL;
    pnewopa->static_variables = NULL;

    if(poldopa->refcount != NULL)
    {
        pnewopa->refcount = (zend_uint *)OMALLOC(popcopy, sizeof(zend_uint));
        if(pnewopa->refcount == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        /* We should make this some really large number, so it can never be deref'd. */
#define BIG_VALUE (64 * 1024)
        *pnewopa->refcount = BIG_VALUE;
    }

    if(poldopa->static_variables != NULL)
    {
        result = copy_hashtable(popcopy, poldopa->static_variables, copy_flag_zval_ref | copy_flag_pDataPtr, &pnewopa->static_variables, NULL, NULL);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
    {
        pnewopa->function_name    = NULL;
        pnewopa->filename         = NULL;
        pnewopa->doc_comment      = NULL;
        pnewopa->arg_info         = NULL;
        pnewopa->brk_cont_array   = NULL;
        pnewopa->try_catch_array  = NULL;
        pnewopa->vars             = NULL;
/*
 * TBD: Scope & Prototype ???
 *       pnewopa->scope            = NULL;
 *       pnewopa->prototype        = NULL;
 */
        /* Copy function_name, filename, doc_comment */
        if(poldopa->function_name != NULL)
        {
            pnewopa->function_name = OSTRDUP(popcopy, poldopa->function_name);
            if(pnewopa->function_name == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }
        }

        if(poldopa->filename != NULL)
        {
            pnewopa->filename = OSTRDUP(popcopy, poldopa->filename);
            if(pnewopa->filename == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }
        }

        if(poldopa->doc_comment != NULL)
        {
            msize = poldopa->doc_comment_len + 1;
            pnewopa->doc_comment = OMALLOC(popcopy, msize);
            if(pnewopa->doc_comment == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            memcpy_s((void *)pnewopa->doc_comment, msize, poldopa->doc_comment, msize);
        }

        /* Copy arg_info, brk_cont_array, try_catch_array and vars */
        if(poldopa->arg_info != NULL)
        {
            result = copy_zend_arg_info_array(popcopy, poldopa->arg_info, poldopa->num_args, &pnewopa->arg_info);
            if(FAILED(result))
            {
                goto Finished;
            }
        }

        if(poldopa->brk_cont_array != NULL)
        {
            msize = poldopa->last_brk_cont * sizeof(zend_brk_cont_element);
            pnewopa->brk_cont_array = (zend_brk_cont_element *)OMALLOC(popcopy, msize);
            if(pnewopa->brk_cont_array == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            memcpy_s(pnewopa->brk_cont_array, msize, poldopa->brk_cont_array, msize);
        }

        if(poldopa->try_catch_array != NULL)
        {
            msize = poldopa->last_try_catch * sizeof(zend_try_catch_element);
            pnewopa->try_catch_array = (zend_try_catch_element *)OMALLOC(popcopy, msize);
            if(pnewopa->try_catch_array == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            memcpy_s(pnewopa->try_catch_array, msize, poldopa->try_catch_array, msize);
        }

/* TBD: Scope & Prototype???
        if (poldopa->scope != NULL)
        {
            result = copy_zend_class_entry(popcopy, poldopa->scope, &pnewopa->scope);
            if (FAILED(result))
            {
                goto Finished;
            }
        }
*/
        /* "prototype" may be undefined if "scope" isn't set */
/*
        if (poldopa->scope && poldopa->prototype)
        {
            result = copy_zend_function(popcopy, poldopa->prototype, &pnewopa->prototype);
            if (FAILED(result))
            {
                goto Finished;
            }
        }
*/
    }

    /* For interened strings, we have to copy both in and out */
    if(poldopa->vars != NULL)
    {
        msize = poldopa->last_var * sizeof(zend_compiled_variable);
        pnewopa->vars = (zend_compiled_variable *)OMALLOC(popcopy, msize);
        if(pnewopa->vars == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        memcpy_s(pnewopa->vars, msize, poldopa->vars, msize);

        for(index = 0; index < poldopa->last_var; index++)
        {
            if(poldopa->vars[index].name != NULL)
            {
                result = wincache_string_pmemcopy(popcopy,
                                                  poldopa->vars[index].name,
                                                  poldopa->vars[index].name_len + 1,
                                                  &pnewopa->vars[index].name);
                if(FAILED(result))
                {
                    goto Finished;
                }

                pnewopa->vars[index].name_len = poldopa->vars[index].name_len;
            }
            else
            {
                pnewopa->vars[index].name = NULL;
                pnewopa->vars[index].name_len = 0;
            }
        }
    }

    /* Allocate memory for opcodes and do a deep copy */
    if(popcopy->optype == OPCOPY_OPERATION_COPYIN ||
       (popcopy->optype == OPCOPY_OPERATION_COPYOUT && (*pdata & oparray_has_const)))
    {
        bMustFixup = 1;
        opcount = poldopa->last;
        pnewopa->opcodes = (zend_op *)OMALLOC(popcopy, sizeof(zend_op) * opcount);
        if(pnewopa->opcodes == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        if(popcopy->optype == OPCOPY_OPERATION_COPYOUT)
        {
            memcpy_s(pnewopa->opcodes, sizeof(zend_op) * opcount, poldopa->opcodes, sizeof(zend_op) * opcount);
        }

        for(index = 0; index < opcount; index++)
        {
            pnewop = &pnewopa->opcodes[index];
            poldop = &poldopa->opcodes[index];

            if(popcopy->optype == OPCOPY_OPERATION_COPYIN
#if !defined( ZEND_ENGINE_2_6 )
               ||
               (popcopy->optype == OPCOPY_OPERATION_COPYOUT &&
#ifdef ZEND_ENGINE_2_4
                ((poldop->op1_type == IS_CONST &&
                  Z_TYPE_P(poldop->op1.zv) == IS_CONSTANT_ARRAY) ||
                (poldop->op2_type == IS_CONST &&
                  Z_TYPE_P(poldop->op2.zv) == IS_CONSTANT_ARRAY)))
#else /* ZEND_ENGINE_2_3 and below */
                ((poldop->op1.op_type == IS_CONST &&
                  poldop->op1.u.constant.type == IS_CONSTANT_ARRAY) ||
                (poldop->op2.op_type == IS_CONST &&
                  poldop->op2.u.constant.type == IS_CONSTANT_ARRAY)))
#endif /* ZEND_ENGINE_2_4 */
#endif /* !defined( ZEND_ENGINE_2_6_1 ) */
               )
            {
                result = copy_zend_op(popcopy, poldop, &pnewop);
                if(FAILED(result))
                {
                    goto Finished;
                }

#if !defined( ZEND_ENGINE_2_6 )
                if(popcopy->optype == OPCOPY_OPERATION_COPYOUT)
                {
                    continue;
                }

                _ASSERT(popcopy->optype == OPCOPY_OPERATION_COPYIN);
                switch(pnewop->opcode)
                {
                    case ZEND_RECV_INIT:
#ifdef ZEND_ENGINE_2_4
                        if(pnewop->op2_type == IS_CONST &&
                           Z_TYPE_P(pnewop->op2.zv) == IS_CONSTANT_ARRAY)
#else /* ZEND_ENGINE_2_3 and below */
                        if(pnewop->op2.op_type == IS_CONST &&
                           pnewop->op2.u.constant.type == IS_CONSTANT_ARRAY)
#endif /* ZEND_ENGINE_2_4 */
                        {
                           *pdata |= oparray_has_const;
                        }
                        break;
                    default:
#ifdef ZEND_ENGINE_2_4
                        if((pnewop->op1_type == IS_CONST &&
                            Z_TYPE_P(pnewop->op1.zv) == IS_CONSTANT_ARRAY) ||
                           (pnewop->op2_type == IS_CONST &&
                            Z_TYPE_P(pnewop->op2.zv) == IS_CONSTANT_ARRAY))
#else /* ZEND_ENGINE_2_3 and below */
                        if((pnewop->op1.op_type == IS_CONST &&
                            pnewop->op1.u.constant.type == IS_CONSTANT_ARRAY) ||
                           (pnewop->op2.op_type == IS_CONST &&
                            pnewop->op2.u.constant.type == IS_CONSTANT_ARRAY))

#endif /* ZEND_ENGINE_2_4 */
                        {
                            *pdata |= oparray_has_const;
                        }
                        break;
                }
#endif /* !defined( ZEND_ENGINE_2_6 ) */
            }
        }
    }

#ifdef ZEND_ENGINE_2_4
    /* Copy the literals */
    /*
     * NOTE: We must copy literals *after* any calls to copy_zend_op(),
     * since copy_zend_op() can change literals.
     */
    if (poldopa->literals)
    {
        zend_literal *src, *dst, *end;

        src = poldopa->literals;
        dst = pnewopa->literals = (zend_literal*) OMALLOC(popcopy, (sizeof(zend_literal) * poldopa->last_literal));
        if (NULL == dst)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
        end = src + poldopa->last_literal;
        while (src < end)
        {
            zval *tmp = &dst->constant;
            *dst = *src; /* struct copy */
            result = copy_zval(popcopy, &src->constant, &tmp);
            if (FAILED(result))
            {
                goto Finished;
            }
            src++;
            dst++;
        }
    }
#endif /* ZEND_ENGINE_2_4 */

    if (bMustFixup)
    {
        /* Walk the opcode array, fixing up offsets */
        for(index = 0; index < opcount; index++)
        {
            pnewop = &pnewopa->opcodes[index];

            /* Fix jump addresses */
            switch(pnewop->opcode)
            {
                case ZEND_JMP:
#ifdef ZEND_ENGINE_2_3
                case ZEND_GOTO:
#endif /* ZEND_ENGINE_2_3 */
#ifdef ZEND_ENGINE_2_5
                case ZEND_FAST_CALL:
#endif /* ZEND_ENGINE_2_5 */
#ifdef ZEND_ENGINE_2_4
                    pnewop->op1.jmp_addr = pnewopa->opcodes + (pnewop->op1.jmp_addr - poldopa->opcodes);
#else /* ZEND_ENGINE_2_3 and below */
                    pnewop->op1.u.jmp_addr = pnewopa->opcodes + (pnewop->op1.u.jmp_addr - poldopa->opcodes);
#endif /* ZEND_ENGINE_2_4 */
                    break;
                case ZEND_JMPZ:
                case ZEND_JMPNZ:
                case ZEND_JMPZ_EX:
                case ZEND_JMPNZ_EX:
#ifdef ZEND_ENGINE_2_3
                case ZEND_JMP_SET:
#endif
#ifdef ZEND_ENGINE_2_4
                case ZEND_JMP_SET_VAR:
#endif /* ZEND_ENGINE_2_4 */
#ifdef ZEND_ENGINE_2_4
                    pnewop->op2.jmp_addr = pnewopa->opcodes + (pnewop->op2.jmp_addr - poldopa->opcodes);
#else /* ZEND_ENGINE_2_3 and below */
                    pnewop->op2.u.jmp_addr = pnewopa->opcodes + (pnewop->op2.u.jmp_addr - poldopa->opcodes);
#endif /* ZEND_ENGINE_2_4 */
                    break;
            }

#ifdef ZEND_ENGINE_2_4
            /* Fixup literal addresses */
            if (pnewop->op1_type == IS_CONST) {
                pnewopa->opcodes[index].op1.literal = poldopa->opcodes[index].op1.literal - poldopa->literals + pnewopa->literals;
            }
            if (pnewop->op2_type == IS_CONST) {
                pnewopa->opcodes[index].op2.literal = poldopa->opcodes[index].op2.literal - poldopa->literals + pnewopa->literals;
            }
            if (pnewop->result_type == IS_CONST) {
                pnewopa->opcodes[index].result.literal = poldopa->opcodes[index].result.literal - poldopa->literals + pnewopa->literals;
            }
#endif /* ZEND_ENGINE_2_4 */
        }
    }

    *ppnewopa = pnewopa;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zend_op_array", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewopa != NULL)
        {
            if(pnewopa->refcount != NULL)
            {
                OFREE(popcopy, pnewopa->refcount);
                pnewopa->refcount = NULL;
            }
        }
    }

    dprintdecorate("end copy_zend_op_array");

    return result;
}

static void free_zend_op_array(opcopy_context * popcopy, zend_op_array * pvalue, unsigned char ffree)
{
    unsigned int index = 0;

    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(pvalue->refcount != NULL)
        {
            OFREE(popcopy, pvalue->refcount);
            pvalue->refcount = NULL;
        }

        if(pvalue->static_variables != NULL)
        {
            free_hashtable(popcopy, pvalue->static_variables, DO_FREE); /* copy_flag_zval_ref | copy_flag_pDataPtr */
            pvalue->static_variables = NULL;
        }

        if(pvalue->function_name != NULL)
        {
            OFREE(popcopy, (void *)pvalue->function_name);
            pvalue->function_name = NULL;
        }

        if(pvalue->filename != NULL)
        {
            OFREE(popcopy, (void *)pvalue->filename);
            pvalue->filename = NULL;
        }

        if(pvalue->doc_comment != NULL)
        {
            OFREE(popcopy, (void *)pvalue->doc_comment);
            pvalue->doc_comment = NULL;
        }

        if(pvalue->arg_info != NULL)
        {
            free_zend_arg_info_array(popcopy, pvalue->arg_info, pvalue->num_args, NO_FREE);

            OFREE(popcopy, pvalue->arg_info);
            pvalue->arg_info = NULL;
        }

        if(pvalue->brk_cont_array != NULL)
        {
            OFREE(popcopy, pvalue->brk_cont_array);
            pvalue->brk_cont_array = NULL;
        }

        if(pvalue->try_catch_array != NULL)
        {
            OFREE(popcopy, pvalue->try_catch_array);
            pvalue->try_catch_array = NULL;
        }

        if(pvalue->vars != NULL)
        {
            OFREE(popcopy, pvalue->vars);
            pvalue->vars = NULL;
        }

        if(pvalue->opcodes != NULL)
        {
            for(index = 0; index < pvalue->last; index++)
            {
                free_zend_op(popcopy, &pvalue->opcodes[index], NO_FREE);
            }

            OFREE(popcopy, pvalue->opcodes);
            pvalue->opcodes = NULL;
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

static int copy_zend_function(opcopy_context * popcopy, zend_function * poldf, zend_function ** ppnewf)
{
    int             result    = NONFATAL;
    int             allocated = 0;
    zend_function * pnewf     = NULL;
    zend_op_array * ptemp     = NULL;

    dprintdecorate("start copy_zend_function");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldf   != NULL);
    _ASSERT(ppnewf  != NULL);

    /* Allocate memory if necessary */
    if(*ppnewf == NULL)
    {
        pnewf = (zend_function *)OMALLOC(popcopy, sizeof(zend_function));
        if(pnewf == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewf = *ppnewf;
    }

    memcpy_s(pnewf, sizeof(zend_function), poldf, sizeof(zend_function));

    switch(poldf->type)
    {
        case ZEND_INTERNAL_FUNCTION:
        case ZEND_OVERLOADED_FUNCTION:
            /* TBD?? Point to a wrapper which will call the process specific */
            /* function pointer. For now just doing pointing to internal op_array */
            pnewf->op_array = poldf->op_array;
            dprintimportant("Internal function oparray pointer = %p %s", poldf->op_array, poldf->common.function_name);
            break;

        case ZEND_USER_FUNCTION:
        case ZEND_EVAL_CODE:
            /* Copy op_array to shared memory */
            ptemp = &pnewf->op_array;
            result = copy_zend_op_array(popcopy, &poldf->op_array, &ptemp);
            if(FAILED(result))
            {
                goto Finished;
            }

            break;

        default:
            _ASSERT(FALSE);
            break;
    }

    /* Take care of union elements properly */
    pnewf->common.prototype = NULL;
    pnewf->common.fn_flags = poldf->common.fn_flags & (~ZEND_ACC_IMPLEMENTED_ABSTRACT);

    *ppnewf = pnewf;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zend_function", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewf != NULL)
        {
            if(allocated != 0)
            {
                OFREE(popcopy, pnewf);
                pnewf = NULL;
            }
        }
    }

    dprintdecorate("end copy_zend_function");

    return result;
}

static void free_zend_function(opcopy_context * popcopy, zend_function * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        switch(pvalue->type)
        {
            case ZEND_INTERNAL_FUNCTION:
            case ZEND_OVERLOADED_FUNCTION:
                    break;

            case ZEND_USER_FUNCTION:
            case ZEND_EVAL_CODE:
                free_zend_op_array(popcopy, &pvalue->op_array, NO_FREE);
                break;
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }
}

static int copy_zend_function_entry(opcopy_context * popcopy, zend_function_entry * poldfe, zend_function_entry ** ppnewfe)
{
    int                    result    = NONFATAL;
    int                    allocated = 0;
    zend_function_entry *  pnewfe    = NULL;

    dprintdecorate("start copy_zend_function_entry");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldfe  != NULL);
    _ASSERT(ppnewfe != NULL);

    /* Allocate memory if required */
    if(*ppnewfe == NULL)
    {
        pnewfe = (zend_function_entry *)OMALLOC(popcopy, sizeof(zend_function_entry));
        if(pnewfe == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewfe = *ppnewfe;
    }

    memcpy_s(pnewfe, sizeof(zend_function_entry), poldfe, sizeof(zend_function_entry));

    pnewfe->fname = NULL;
    pnewfe->arg_info = NULL;

    /* TBD?? handler function pointer can be different in different processes */

    if(poldfe->fname != NULL)
    {
        pnewfe->fname = OSTRDUP(popcopy, poldfe->fname);
        if(pnewfe->fname == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
    }

    if(poldfe->arg_info != NULL)
    {
        result = copy_zend_arg_info(popcopy, (zend_arg_info *)poldfe->arg_info, (zend_arg_info **)&pnewfe->arg_info);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    *ppnewfe = pnewfe;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(result != NONFATAL)
    {
        dprintimportant("failure %d in copy_zend_function_entry", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewfe !=  NULL)
        {
            if(pnewfe->fname != NULL)
            {
                OFREE(popcopy, (void *)pnewfe->fname);
                pnewfe->fname = NULL;
            }

            if(pnewfe->arg_info != NULL)
            {
                OFREE(popcopy, (void *)pnewfe->arg_info);
                pnewfe->arg_info = NULL;
            }

            if(allocated != 0)
            {
                OFREE(popcopy, pnewfe);
                pnewfe = NULL;
            }
        }
    }

    dprintdecorate("end copy_zend_function_entry");

    return result;
}

static void free_zend_function_entry(opcopy_context * popcopy, zend_function_entry * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(pvalue->fname != NULL)
        {
            OFREE(popcopy, (void *)pvalue->fname);
            pvalue->fname = NULL;
        }

        if(pvalue->arg_info != NULL)
        {
            free_zend_arg_info(popcopy, (zend_arg_info *)pvalue->arg_info, DO_FREE);
            pvalue->arg_info = NULL;
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }
}

static int copy_hashtable(opcopy_context * popcopy, HashTable * poldh, unsigned int copy_flag, HashTable ** ppnewh, void * parg1, void * parg2)
{
    int          result    = NONFATAL;
    int          allocated = 0;
    unsigned int index     = 0;
    unsigned int bnum      = 0;

    HashTable *  pnewh     = NULL;
    Bucket *     pbucket   = NULL;
    Bucket *     plast     = NULL;
    Bucket *     ptemp     = NULL;

    dprintverbose("start copy_hashtable");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldh   != NULL);
    _ASSERT(ppnewh  != NULL);

    /* Allocate memory for HashTable if not already allocated */
    if(*ppnewh == NULL)
    {
        pnewh = (HashTable *)OMALLOC(popcopy, sizeof(HashTable));
        if(pnewh == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewh = *ppnewh;
    }

    /* TBD?? dtor_func_t pointer can be different in different processes */

    /* Do memcpy of HashTable first */
    memcpy_s(pnewh, sizeof(HashTable), poldh, sizeof(HashTable));

    pnewh->pInternalPointer = NULL;
    pnewh->pListHead        = NULL;
    pnewh->pListTail        = NULL;
    pnewh->arBuckets        = NULL;

    /* Allocate memory for bucket pointers and set them to NULL */
    pnewh->arBuckets = (Bucket **)OMALLOC(popcopy, poldh->nTableSize * sizeof(Bucket *));
    if(pnewh->arBuckets == NULL)
    {
        result = popcopy->oomcode;
        goto Finished;
    }

    for(index = 0; index < poldh->nTableSize; index++)
    {
        pnewh->arBuckets[index] = NULL;
    }

    /* Traverse the hashtable and copy the buckets */
    index = 0;
    pbucket = poldh->pListHead;
    plast = NULL;
    ptemp = NULL;
    while(pbucket != NULL)
    {
        /* Check if this bucket need to be copied */
        if(copy_flag & copy_flag_docheck)
        {
            if(!check_hashtable_bucket(pbucket, copy_flag, parg1, parg2))
            {
                pnewh->nNumOfElements--;
                pbucket = pbucket->pListNext;
                continue;
            }
        }

        ptemp = NULL;
        result = copy_hashtable_bucket(popcopy, pbucket, copy_flag, &ptemp);
        if(FAILED(result))
        {
            goto Finished;
        }

        if(copy_flag & copy_flag_pDataPtr)
        {
            memcpy_s(&ptemp->pDataPtr, sizeof(void *), ptemp->pData, sizeof(void *));
        }
        else
        {
            ptemp->pDataPtr = NULL;
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

        plast = ptemp;
        pbucket = pbucket->pListNext;
    }

    pnewh->pListTail = ptemp;

    zend_hash_internal_pointer_reset(pnewh);
    *ppnewh = pnewh;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_hashtable", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewh != NULL)
        {
            if(pnewh->arBuckets != NULL)
            {
                OFREE(popcopy, pnewh->arBuckets);
                pnewh->arBuckets = NULL;
            }

            if(allocated != 0)
            {
                OFREE(popcopy, pnewh);
                pnewh = NULL;
            }
        }
    }

    dprintverbose("end copy_hashtable");

    return result;
}

static void free_hashtable(opcopy_context * popcopy, HashTable * pvalue, unsigned char ffree)
{
    Bucket * pbucket = NULL;
    Bucket * ptemp   = NULL;

    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);

        pbucket = pvalue->pListHead;
        while(pbucket != NULL)
        {
            ptemp = pbucket;
            pbucket = pbucket->pListNext;

            free_hashtable_bucket(popcopy, ptemp, DO_FREE);
            ptemp = NULL;
        }

        if(pvalue->arBuckets != NULL)
        {
            OFREE(popcopy, pvalue->arBuckets);
            pvalue->arBuckets = NULL;
        }

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

static int check_hashtable_bucket(Bucket * pbucket, unsigned int copy_flag, void * arg1, void * arg2)
{
    int required = 1; /* TBD?? Change to 0 and complete this method */

    zend_class_entry *   pclass    = NULL;
    zend_class_entry *   pparent   = NULL;
    zend_function *      pfunc     = NULL;

    zval **              ppzval    = NULL;
    zval **              ppparentz = NULL;
    zend_property_info * pproperty = NULL;
    zend_property_info * pparentp  = NULL;
    HashTable *          phasht    = NULL;
    HashTable *          phashp    = NULL;

    dprintdecorate("start check_hashtable_bucket");

    _ASSERT(pbucket   != NULL);
    _ASSERT(copy_flag != 0);
    _ASSERT(arg1      != NULL);

    if(copy_flag & copy_flag_zend_func)
    {
        _ASSERT(required == 1);

        pclass = (zend_class_entry *)arg1;
        pfunc = (zend_function *)pbucket->pData;

        if(pfunc->common.scope != pclass)
        {
            required = 0;
        }
    }

#if !defined(ZEND_ENGINE_2_4)
    if(copy_flag & copy_flag_def_prop)
    {
        _ASSERT(required == 1);

        pclass = (zend_class_entry *)arg1;
        pparent = pclass->parent;
        ppzval = (zval **)pbucket->pData;

        if(pparent != NULL && zend_hash_quick_find(&pparent->default_properties, pbucket->arKey, pbucket->nKeyLength, pbucket->h, (void **)&ppparentz) == SUCCESS)
        {
            if(ppparentz && ppzval && (*ppparentz == *ppzval))
            {
                required = 0;
            }
        }
    }
#endif /* !defined(ZEND_ENGINE_2_4) */

    if(copy_flag & copy_flag_prop_info)
    {
        _ASSERT(required == 1);

        pclass = (zend_class_entry *)arg1;
        pparent = pclass->parent;
        pproperty = (zend_property_info *)pbucket->pData;

        /* Making decision on versions before PHP_VERSION_52 is hard */
        /* but luckily we only support PHP version 5.2 and beyond */
        if(pproperty->ce != pclass)
        {
            required = 0;
        }
    }

#if !defined(ZEND_ENGINE_2_4)
    if(copy_flag & copy_flag_stat_var)
    {
        _ASSERT(required == 1);

        pclass = (zend_class_entry *)arg1;
        phasht = (HashTable *)arg2;
        ppzval = (zval **)pbucket->pData;

        pparent = pclass->parent;
        /* If parent class is NULL, nothing more to do */
        if(pparent != NULL)
        {
            /* If both properties point to same zval, set required to 0 */
            phashp = pparent->static_members;
            if(phasht == &pclass->default_static_members)
            {
                phashp = &pparent->default_static_members;
            }

            if(zend_hash_quick_find(phashp, pbucket->arKey, pbucket->nKeyLength, pbucket->h, (void **)&ppparentz) == SUCCESS)
            {
                if(*ppparentz == *ppzval)
                {
                    required = 0;
                }
            }
        }
    }
#endif /* !defined(ZEND_ENGINE_2_4) */

    dprintdecorate("end check_hashtable_bucket");

    return required;
}

static int copy_hashtable_bucket(opcopy_context * popcopy, Bucket * poldb, unsigned int copy_flag, Bucket ** ppnewb)
{
    int      result    = NONFATAL;
    int      allocated = 0; /* This is a bitmask in ZEND_ENGINE_2_4: 1 for the Bucket, 2 for the Bucket.arKey */
    Bucket * pnewb     = NULL;
    int      msize     = 0;

    dprintdecorate("start copy_hashtable_bucket");

    _ASSERT(popcopy   != NULL);
    _ASSERT(poldb     != NULL);
    _ASSERT(ppnewb    != NULL);

#ifdef ZEND_ENGINE_2_4
    if (*ppnewb == NULL)
    {
        pnewb = (Bucket *)OMALLOC(popcopy, sizeof(Bucket));
        if (pnewb == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
        /* Remember that we've alloc'd the bucket */
        allocated = 1;
    }
    memcpy_s(pnewb, sizeof(Bucket), poldb, sizeof(Bucket));

    /* In some cases, poldb->nKeyLength can be zero. */
    if (poldb->nKeyLength)
    {
        result  = wincache_string_pmemcopy(popcopy,
                                           poldb->arKey,
                                           poldb->nKeyLength+1,
                                           &pnewb->arKey);
        if (FAILED(result))
        {
            goto Finished;
        }

        /* Remember that we've alloc'd the key string */
        allocated |= 2;
    }
#else /* ZEND_ENGINE_2_3 and below */
    /*
     * NOTE: This assumes the string for arKey is contiguously alloc'd within
     * the poldb Bucket.
     */
    msize = sizeof(Bucket) + poldb->nKeyLength - 1;
    if(*ppnewb == NULL)
    {
        pnewb = (Bucket *)OMALLOC(popcopy, msize);
        if(pnewb == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }

    /* Copy hashcode value and set pointers to NULL */
    _ASSERT(pnewb != NULL);
    memcpy_s(pnewb, msize, poldb, msize);
#endif /* ZEND_ENGINE_2_4 */

    pnewb->pData      = NULL;
    pnewb->pDataPtr   = NULL;
    pnewb->pListLast  = NULL;
    pnewb->pListNext  = NULL;
    pnewb->pLast      = NULL;
    pnewb->pNext      = NULL;

    /* Depending on what kind of data is stored in buckets */
    /* call different copy functions to copy those */
    if(copy_flag & copy_flag_zend_func)
    {
        result = copy_zend_function(popcopy, (zend_function *)poldb->pData, (zend_function **)&pnewb->pData);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    if(copy_flag & copy_flag_zval_ref)
    {
        result = copy_zval_ref(popcopy, (zval **)poldb->pData, (zval ***)&pnewb->pData);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    if(copy_flag & copy_flag_prop_info)
    {
        result = copy_zend_property_info(popcopy, (zend_property_info *)poldb->pData, (zend_property_info **)&pnewb->pData);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    _ASSERT(pnewb->pData != NULL);

    *ppnewb = pnewb;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_hashtable_bucket", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewb != NULL)
        {
#ifdef ZEND_ENGINE_2_4
            /* Check if we've alloc'd the key string */
            if (allocated & 2)
            {
                if(!IS_INTERNED(pnewb->arKey))
                {
                    OFREE(popcopy, (char *)pnewb->arKey);
                }
            }
#endif /* ZEND_ENGINE_2_4 */
            /* Check if we've alloc'd the bucket */
            if (allocated & 1)
            {
                OFREE(popcopy, pnewb);
                pnewb = NULL;
            }
        }
    }

    dprintdecorate("end copy_hashtable_bucket");

    return result;
}

static void free_hashtable_bucket(opcopy_context * popcopy, Bucket * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(ffree == DO_FREE)
        {
#ifdef ZEND_ENGINE_2_4
            if (pvalue->nKeyLength && pvalue->arKey &&
                !IS_INTERNED(pvalue->arKey) &&
                !IS_TAILED(pvalue, arKey))
            {
                OFREE(popcopy, (char *)pvalue->arKey);
            }
#endif /* ZEND_ENGINE_2_4 */
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

static int copy_zend_class_entry(opcopy_context * popcopy, zend_class_entry * poldce, zend_class_entry ** ppnewce)
{
    int                result    = NONFATAL;
    int                allocated = 0;
    unsigned int       index     = 0;
    unsigned int       count     = 0;
    unsigned int       checkflag = 0;
    unsigned int       msize     = 0;

    zend_class_entry *    pnewce    = NULL;
    zend_function_entry * pfunc     = NULL;
    zend_function *       pfunction = NULL;
    zend_property_info *  pproperty = NULL;
    HashTable *           phasht    = NULL;
    Bucket *              pbucket   = NULL;
    zval *                pzval     = NULL;

    dprintverbose("start copy_zend_class_entry");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldce  != NULL);
    _ASSERT(ppnewce != NULL);

    /* Allocated memory if not already allocated */
    if(*ppnewce == NULL)
    {
        pnewce = (zend_class_entry *)OMALLOC(popcopy, sizeof(zend_class_entry));
        if(pnewce == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewce = *ppnewce;
    }

    memcpy_s(pnewce, sizeof(zend_class_entry), poldce, sizeof(zend_class_entry));

    pnewce->name = NULL;
    memset(&pnewce->function_table, 0, sizeof(HashTable));
    memset(&pnewce->properties_info, 0, sizeof(HashTable));
    memset(&pnewce->constants_table, 0, sizeof(HashTable));
#ifdef ZEND_ENGINE_2_4
    if (poldce->type == ZEND_USER_CLASS) {
        pnewce->info.user.doc_comment = NULL;
        pnewce->info.user.filename = NULL;
    } else {
        _ASSERT(poldce->type == ZEND_INTERNAL_CLASS);
        pnewce->info.internal.builtin_functions = NULL;
        /* NOTE: No clue what we're supposed to do with the info.internal.module */
    }
    pnewce->static_members_table = NULL;
    pnewce->default_properties_count = 0;
    pnewce->default_properties_table = NULL;
    pnewce->default_static_members_count = 0;
    pnewce->default_static_members_table = NULL;
#else /* ZEND_ENGINE_2_3 and below */
    pnewce->doc_comment = NULL;
    pnewce->filename = NULL;
    pnewce->static_members = NULL;
    pnewce->builtin_functions = NULL;
    memset(&pnewce->default_properties, 0, sizeof(HashTable));
    memset(&pnewce->default_static_members, 0, sizeof(HashTable));
#endif /* ZEND_ENGINE_2_4 */

    if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
    {
        /* Don't check_hashtable when copying out */
        checkflag = copy_flag_docheck;
    }

    /* Copy name, doc_comment and filename */
    if(poldce->name != NULL)
    {
        /* NOTE: The name is apparently not a candidate for being 'interned' */
        pnewce->name = OSTRDUP(popcopy, poldce->name);
        if(pnewce->name == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
    }

#ifdef ZEND_ENGINE_2_4
    if(poldce->type == ZEND_USER_CLASS && poldce->info.user.doc_comment != NULL)
    {
        /* NOTE: The doc_comment is apparently not a candidate for being 'interned' */
        msize = poldce->info.user.doc_comment_len + 1;
        pnewce->info.user.doc_comment = OMALLOC(popcopy, msize);
        if(pnewce->info.user.doc_comment == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
        /* NOTE: info.user.doc_comment_len already copied by struct copy, above */
        memcpy_s((void *)pnewce->info.user.doc_comment, msize, poldce->info.user.doc_comment, msize);
    }
#else /* ZEND_ENGINE_2_3 and below */
    if(poldce->doc_comment != NULL)
    {
        msize = poldce->doc_comment_len + 1;
        pnewce->doc_comment = OMALLOC(popcopy, msize);
        if(pnewce->doc_comment == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        memcpy_s(pnewce->doc_comment, msize, poldce->doc_comment, msize);
    }
#endif /* ZEND_ENGINE_2_4 */

#ifdef ZEND_ENGINE_2_4
    if(poldce->type == ZEND_USER_CLASS && poldce->info.user.filename != NULL)
    {
        /* NOTE: The filename is apparently not a candidate for being 'interned' */
        pnewce->info.user.filename = OSTRDUP(popcopy, poldce->info.user.filename);
        if(pnewce->info.user.filename == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
    }
#else /* ZEND_ENGINE_2_3 and below */
    if(poldce->filename != NULL)
    {
       pnewce->filename = OSTRDUP(popcopy, poldce->filename);
        if(pnewce->filename == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }
    }
#endif /* ZEND_ENGINE_2_4 */

    /* Calculate num_interfaces not including inherited ones */
    pnewce->interfaces = NULL;
    if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
    {
        for(index = 0; index < poldce->num_interfaces; index++)
        {
            if(poldce->interfaces[index] != NULL)
            {
                pnewce->num_interfaces = index;
                break;
            }
        }
    }
    else if(popcopy->optype == OPCOPY_OPERATION_COPYOUT)
    {
        if(poldce->num_interfaces > 0)
        {
            pnewce->interfaces = OMALLOC(popcopy, sizeof(zend_class_entry *) * poldce->num_interfaces);
            if(pnewce->interfaces == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            memset(pnewce->interfaces, 0, sizeof(zend_class_entry *) * poldce->num_interfaces);
        }
    }
    else
    {
        _ASSERT(FALSE);
    }

    /* Copy function_table and also do fixup. pDataPtr is not useful */
    phasht = &pnewce->function_table;
    result = copy_hashtable(popcopy, &poldce->function_table, copy_flag_zend_func | checkflag, &phasht, poldce, NULL);
    if(FAILED(result))
    {
        goto Finished;
    }

    pnewce->parent           = NULL;
    pnewce->constructor      = NULL;
    pnewce->destructor       = NULL;
    pnewce->clone            = NULL;
    pnewce->__get            = NULL;
    pnewce->__set            = NULL;
    pnewce->__unset          = NULL;
    pnewce->__isset          = NULL;
    pnewce->__call           = NULL;
#ifdef ZEND_ENGINE_2_3
    pnewce->__callstatic     = NULL;
#endif
    pnewce->__tostring       = NULL;
    pnewce->serialize_func   = NULL;
    pnewce->unserialize_func = NULL;
#ifdef ZEND_ENGINE_2_6
    pnewce->__debugInfo      = NULL;
#endif

    // If ZEND_ENGINE_2_4, copy the trait_aliases and trait_precedences
#ifdef ZEND_ENGINE_2_4
    if (poldce->trait_aliases) {
        int i = 0;
        int num_trait_aliases = 0;

        /* get how much trait aliases we've got */
        while (poldce->trait_aliases[num_trait_aliases]) {num_trait_aliases++;}

        /* copy all the trait aliases */
        CHECK(pnewce->trait_aliases =
                (zend_trait_alias **) OMALLOC(popcopy, sizeof(zend_trait_alias *)*(num_trait_aliases+1)));

        i = 0;
        while (poldce->trait_aliases[i] && i < num_trait_aliases) {
            pnewce->trait_aliases[i] = opcopy_trait_alias(popcopy, poldce->trait_aliases[i]);
            CHECK(pnewce->trait_aliases[i]);
            i++;
        }
        pnewce->trait_aliases[i] = NULL;
    }
    if (poldce->trait_precedences) {
        int i = 0;
        int num_trait_precedences = 0;

        /* get how much trait precedences we've got */
        while (poldce->trait_precedences[num_trait_precedences]) {num_trait_precedences++;}

        /* copy all the trait precedences */
        CHECK(pnewce->trait_precedences =
                (zend_trait_precedence **) OMALLOC(popcopy, sizeof(zend_trait_precedence *)*(num_trait_precedences+1)));

        i = 0;
        while (poldce->trait_precedences[i] && i < num_trait_precedences) {
            pnewce->trait_precedences[i] = opcopy_trait_precedence(popcopy, poldce->trait_precedences[i]);
            CHECK(pnewce->trait_precedences[i]);
            i++;
        }
        pnewce->trait_precedences[i] = NULL;
    }
#endif

    for (index = 0; index < phasht->nTableSize; index++)
    {
        if(phasht->arBuckets == NULL)
        {
            break;
        }

        pbucket = phasht->arBuckets[index];
        while (pbucket != NULL)
        {
            pfunction = (zend_function *)pbucket->pData;
            if(pfunction->common.scope == poldce)
            {
                if(pfunction->common.fn_flags & ZEND_ACC_CTOR)
                {
                    pnewce->constructor = pfunction;
                }
                else if(pfunction->common.fn_flags & ZEND_ACC_DTOR)
                {
                    pnewce->destructor = pfunction;
                }
                else if(pfunction->common.fn_flags & ZEND_ACC_CLONE)
                {
                    pnewce->clone = pfunction;
                }
                else
                {
                    if(poldce->__get && strcmp(pfunction->common.function_name, poldce->__get->common.function_name) == 0)
                    {
                        pnewce->__get = pfunction;
                    }
                    if(poldce->__set && strcmp(pfunction->common.function_name, poldce->__set->common.function_name) == 0)
                    {
                        pnewce->__set = pfunction;
                    }
                    if(poldce->__unset && strcmp(pfunction->common.function_name, poldce->__unset->common.function_name) == 0)
                    {
                        pnewce->__unset = pfunction;
                    }
                    if(poldce->__isset && strcmp(pfunction->common.function_name, poldce->__isset->common.function_name) == 0)
                    {
                        pnewce->__isset = pfunction;
                    }
                    if(poldce->__call && strcmp(pfunction->common.function_name, poldce->__call->common.function_name) == 0)
                    {
                        pnewce->__call = pfunction;
                    }
#ifdef ZEND_ENGINE_2_3
                    if(poldce->__callstatic && strcmp(pfunction->common.function_name, poldce->__callstatic->common.function_name) == 0)
                    {
                        pnewce->__callstatic = pfunction;
                    }
#endif
                    if(poldce->__tostring && strcmp(pfunction->common.function_name, poldce->__tostring->common.function_name) == 0)
                    {
                        pnewce->__tostring = pfunction;
                    }
#ifdef ZEND_ENGINE_2_6
                    if(poldce->__debugInfo && strcmp(pfunction->common.function_name, poldce->__debugInfo->common.function_name) == 0)
                    {
                        pnewce->__debugInfo = pfunction;
                    }
#endif
                }

                pfunction->common.scope = pnewce;
            }

            pbucket = pbucket->pNext;
        }
    }

#ifdef ZEND_ENGINE_2_4
    if (poldce->default_properties_table)
    {
        result = copy_zval_array(popcopy, poldce->default_properties_table, poldce->default_properties_count, &pnewce->default_properties_table);
        if(FAILED(result))
        {
            goto Finished;
        }
        pnewce->default_properties_count = poldce->default_properties_count;
    }
#else /* ZEND_ENGINE_2_3 and below */
    /* Copy default properties. pDataPtr is significant */
    phasht = &pnewce->default_properties;
    result = copy_hashtable(popcopy, &poldce->default_properties, copy_flag_zval_ref | copy_flag_pDataPtr | copy_flag_def_prop | checkflag, &phasht, poldce, NULL);
    if(FAILED(result))
    {
        goto Finished;
    }
#endif /* ZEND_ENGINE_2_4 */

    /* Copy property_info and do fixup */
    phasht = &pnewce->properties_info;
    result = copy_hashtable(popcopy, &poldce->properties_info, copy_flag_prop_info | checkflag, &phasht, poldce, NULL);
    if(FAILED(result))
    {
        goto Finished;
    }

    for (index = 0; index < phasht->nTableSize; index++)
    {
        if(phasht->arBuckets == NULL)
        {
                break;
        }

        pbucket = phasht->arBuckets[index];
        while (pbucket != NULL)
        {
            pproperty = (zend_property_info *)pbucket->pData;
            if(pproperty->ce == poldce)
            {
                pproperty->ce = pnewce;
            }

            pbucket = pbucket->pNext;
        }
    }

    /* Copy default_static_members and static_members */
#ifdef ZEND_ENGINE_2_4
    if (poldce->default_static_members_table)
    {
        _ASSERT(poldce->default_static_members_count > 0);
        result = copy_zval_array(popcopy, poldce->default_static_members_table, poldce->default_static_members_count, &pnewce->default_static_members_table);
        if(FAILED(result))
        {
            goto Finished;
        }
        pnewce->default_static_members_count = poldce->default_static_members_count;
    }

    if(poldce->static_members_table)
    {
        _ASSERT(poldce->default_static_members_count > 0);
        if(poldce->static_members_table != poldce->default_static_members_table)
        {
            result = copy_zval_array(popcopy, poldce->static_members_table, poldce->default_static_members_count, &pnewce->static_members_table);
            if(FAILED(result))
            {
                goto Finished;
            }
        }
        else
        {
            pnewce->static_members_table = pnewce->default_static_members_table;
            /* Note: count of entries is held in default_static_members_count. */
        }
    }
#else /* ZEND_ENGINE_2_3 and below */
    phasht = &pnewce->default_static_members;
    result = copy_hashtable(popcopy, &poldce->default_static_members, copy_flag_zval_ref | copy_flag_stat_var | copy_flag_pDataPtr | checkflag, &phasht, poldce, &poldce->default_static_members);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(poldce->static_members != &poldce->default_static_members)
    {
        result = copy_hashtable(popcopy, poldce->static_members, copy_flag_zval_ref | copy_flag_pDataPtr | copy_flag_stat_var | checkflag, &pnewce->static_members, poldce, poldce->static_members);
        if(FAILED(result))
        {
            goto Finished;
        }
    }
    else
    {
        pnewce->static_members = &pnewce->default_static_members;
    }
#endif /* ZEND_ENGINE_2_4 */

    /* Copy constants */
    phasht = &pnewce->constants_table;
    result = copy_hashtable(popcopy, &poldce->constants_table, copy_flag_zval_ref | copy_flag_pDataPtr, &phasht, NULL, NULL);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Copy built_in functions */
#ifdef ZEND_ENGINE_2_4
    if (poldce->type == ZEND_INTERNAL_CLASS) {
        if(popcopy->optype == OPCOPY_OPERATION_COPYIN && poldce->info.internal.builtin_functions != NULL)
        {
            for(count = 0; poldce->info.internal.builtin_functions[count].fname != NULL; count++)
            {
                /* build up 'count' so we can set the builtin_functions pointer correctly */
            }

            pnewce->info.internal.builtin_functions = (zend_function_entry *)OMALLOC(popcopy, (count + 1) * sizeof(zend_function_entry));
            if(pnewce->info.internal.builtin_functions == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            for(index = 0; index < count; index++)
            {
                pfunc = (zend_function_entry *)&pnewce->info.internal.builtin_functions[index];
                result = copy_zend_function_entry(popcopy, (zend_function_entry *)&poldce->info.internal.builtin_functions[index], &pfunc);
                if(FAILED(result))
                {
                     goto Finished;
                }
            }

            *(char **)&(pnewce->info.internal.builtin_functions[count].fname) = NULL;
        }
    }
#else /* ZEND_ENGINE_2_3 and below */
    if(popcopy->optype == OPCOPY_OPERATION_COPYIN && poldce->builtin_functions != NULL)
    {
        for(count = 0; poldce->type == ZEND_INTERNAL_CLASS && poldce->builtin_functions[count].fname != NULL; count++)
        {
        }

        pnewce->builtin_functions = (zend_function_entry *)OMALLOC(popcopy, (count + 1) * sizeof(zend_function_entry));
        if(pnewce->builtin_functions == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        for(index = 0; index < count; index++)
        {
            pfunc = (zend_function_entry *)&pnewce->builtin_functions[index];
            result = copy_zend_function_entry(popcopy, (zend_function_entry *)&poldce->builtin_functions[index], &pfunc);
            if(FAILED(result))
            {
                 goto Finished;
            }
        }

        *(char **)&(pnewce->builtin_functions[count].fname) = NULL;
    }
#endif /* ZEND_ENGINE_2_4 */

    *ppnewce = pnewce;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zend_class_entry", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end copy_zend_class_entry");

    return result;
}

static void free_zend_class_entry(opcopy_context * popcopy, zend_class_entry * pvalue, unsigned char ffree)
{
    if(pvalue != NULL)
    {
        _ASSERT(popcopy->palloc != NULL);
        if(pvalue->name != NULL)
        {
            OFREE(popcopy, (void *)pvalue->name);
            pvalue->name = NULL;
        }

        free_hashtable(popcopy, &pvalue->function_table, NO_FREE); /* copy_flag_zend_func | checkflag */
        free_hashtable(popcopy, &pvalue->constants_table, NO_FREE);

#ifdef ZEND_ENGINE_2_4
        if(pvalue->info.user.doc_comment != NULL)
        {
            OFREE(popcopy, (void *)pvalue->info.user.doc_comment);
            pvalue->info.user.doc_comment = NULL;
        }

        if(pvalue->info.user.filename != NULL)
        {
            OFREE(popcopy, (void *)pvalue->info.user.filename);
            pvalue->info.user.filename = NULL;
        }

        free_zval_array(popcopy, pvalue->default_properties_table, pvalue->default_properties_count, NO_FREE);
        free_zval_array(popcopy, pvalue->default_static_members_table, pvalue->default_static_members_count, NO_FREE);

        if(pvalue->static_members_table != pvalue->default_static_members_table)
        {
            free_zval_array(popcopy, pvalue->static_members_table, pvalue->default_static_members_count, DO_FREE);
            pvalue->static_members_table = NULL;
        }

        if(pvalue->info.internal.builtin_functions != NULL)
        {
            OFREE(popcopy, (void *)pvalue->info.internal.builtin_functions);
            pvalue->info.internal.builtin_functions = NULL;
        }
#else /* ZEND_ENGINE_2_3 and below */
        if(pvalue->doc_comment != NULL)
        {
            OFREE(popcopy, (void *)pvalue->doc_comment);
            pvalue->doc_comment = NULL;
        }

        if(pvalue->filename != NULL)
        {
            OFREE(popcopy, (void *)pvalue->filename);
            pvalue->filename = NULL;
        }

        free_hashtable(popcopy, &pvalue->default_properties, NO_FREE); /* copy_flag_zval_ref | copy_flag_pDataPtr | copy_flag_def_prop | checkflag */
        free_hashtable(popcopy, &pvalue->default_static_members, NO_FREE); /* copy_flag_prop_info | checkflag */

        if(pvalue->static_members != &pvalue->default_static_members)
        {
            free_hashtable(popcopy, pvalue->static_members, DO_FREE);
            pvalue->static_members = NULL;
        }

        if(pvalue->builtin_functions != NULL)
        {
            OFREE(popcopy, (void *)pvalue->builtin_functions);
            pvalue->builtin_functions = NULL;
        }
#endif /* ZEND_ENGINE_2_4 */

// TODO: free Trait_aliases & Trait_precedences

        if(ffree == DO_FREE)
        {
            OFREE(popcopy, pvalue);
            pvalue = NULL;
        }
    }

    return;
}

static int copy_zend_constant_entry(opcopy_context * popcopy, zend_constant * poldt, zend_constant ** ppnewt)
{
    int             result    = NONFATAL;
    int             allocated = 0;
    zend_constant * pnewt     = NULL;
    unsigned int    namelen   = 0;
    zval *          pzval     = NULL;

    _ASSERT(popcopy != NULL);
    _ASSERT(poldt   != NULL);
    _ASSERT(ppnewt  != NULL);

    dprintverbose("start copy_zend_constant_entry");

    /* Allocate memory if neccessary */
    if(*ppnewt == NULL)
    {
        pnewt = (zend_constant *)OMALLOC(popcopy, sizeof(zend_constant));
        if(pnewt == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        allocated = 1;
    }
    else
    {
        pnewt = *ppnewt;
    }

    /* Do a blind copy before doing deep copy of zval, name */
    memcpy_s(pnewt, sizeof(zend_constant), poldt, sizeof(zend_constant));

    /* Copy the name */
    pnewt->name = NULL;

    if(poldt->name != NULL)
    {
        result = wincache_string_pmemcopy(popcopy, poldt->name, poldt->name_len + 1, &pnewt->name);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    /* Do a deep copy for copyin. For copyout, shallow copy is enough */
    if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
    {
        pzval = &pnewt->value;
        result = copy_zval(popcopy, &poldt->value, &pzval);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    *ppnewt = pnewt;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copy_zend_constant_entry", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnewt != NULL)
        {
            if(popcopy->optype == OPCOPY_OPERATION_COPYIN)
            {
                if(pnewt->name != NULL)
                {
                    OFREE(popcopy, pnewt->name);
                    pnewt->name = NULL;
                }
            }

            if(allocated != 0)
            {
                OFREE(popcopy, pnewt);
                pnewt = NULL;
            }
        }
    }

    dprintverbose("end copy_zend_constant_entry");

    return result;
}

int opcopy_create(opcopy_context ** ppopcopy)
{
    int              result  = NONFATAL;
    opcopy_context * popcopy = NULL;

    dprintverbose("start opcopy_create");

    _ASSERT(ppopcopy != NULL);
    *ppopcopy = NULL;

    popcopy = (opcopy_context *)alloc_pemalloc(sizeof(opcopy_context));
    if(popcopy == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    popcopy->oldfncount = 0;
    popcopy->newfncount = 0;
    popcopy->oldclcount = 0;
    popcopy->newclcount = 0;
    popcopy->oldtncount = 0;
    popcopy->newtncount = 0;
    popcopy->oparray    = NULL;

    popcopy->optype     = OPCOPY_OPERATION_INVALID;
    popcopy->oomcode    = 0;
    popcopy->palloc     = NULL;
    popcopy->hoffset    = 0;
    popcopy->fnmalloc   = NULL;
    popcopy->fnrealloc  = NULL;
    popcopy->fnstrdup   = NULL;
    popcopy->fnfree     = NULL;

    *ppopcopy = popcopy;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in opcopy_create", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end opcopy_create");

    return result;
}

void opcopy_destroy(opcopy_context * popcopy)
{
    dprintverbose("start opcopy_destroy");

    if(popcopy != NULL)
    {
        alloc_pefree(popcopy);
        popcopy = NULL;
    }

    dprintverbose("end opcopy_destroy");

    return;
}

int opcopy_initialize(opcopy_context * popcopy, alloc_context * palloc, size_t hoffset, int resnumber)
{
    int result = NONFATAL;

    dprintverbose("start opcopy_initialize");

    _ASSERT(popcopy   != NULL);
    _ASSERT(resnumber != -1);

    popcopy->palloc    = palloc;
    popcopy->hoffset   = hoffset;
    popcopy->resnumber = resnumber;

    if(palloc != NULL)
    {
        popcopy->oomcode   = FATAL_OUT_OF_SMEMORY;
        popcopy->fnmalloc  = alloc_ommalloc;
        popcopy->fnrealloc = alloc_omrealloc;
        popcopy->fnstrdup  = alloc_omstrdup;
        popcopy->fnfree    = alloc_omfree;
    }
    else
    {
        popcopy->oomcode   = FATAL_OUT_OF_LMEMORY;
        popcopy->fnmalloc  = alloc_oemalloc;
        popcopy->fnrealloc = alloc_oerealloc;
        popcopy->fnstrdup  = alloc_oestrdup;
        popcopy->fnfree    = alloc_oefree;
    }

    dprintverbose("end opcopy_initialize");

    return result;
}

void opcopy_terminate(opcopy_context * popcopy)
{
    dprintverbose("start opcopy_terminate");

    if(popcopy != NULL)
    {
        popcopy->palloc    = NULL;
        popcopy->hoffset   = 0;
        popcopy->fnmalloc  = NULL;
        popcopy->fnrealloc = NULL;
        popcopy->fnstrdup  = NULL;
        popcopy->fnfree    = NULL;
    }

    dprintverbose("end opcopy_terminate");

    return;
}

static int copyin_zend_op_array(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int result = NONFATAL;
    dprintverbose("start copyin_zend_op_array");

    _ASSERT(popcopy          != NULL);
    _ASSERT(popcopy->oparray != NULL);
    _ASSERT(pvalue           != NULL);

    result = copy_zend_op_array(popcopy, popcopy->oparray, &pvalue->oparray);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_zend_op_array", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end copyin_zend_op_array");

    return result;
}

static int copyin_zend_functions(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int          result   = NONFATAL;
    unsigned int newcount = 0;
    unsigned int index    = 0;
    char *       fname    = NULL;
    unsigned int fnamelen = 0;

    zend_function *         ozendfn = NULL;
    zend_function *         nzendfn = NULL;
    ocache_user_function *  fnvalue = NULL;

    dprintverbose("start copyin_zend_functions");

    _ASSERT(popcopy != NULL);
    _ASSERT(pvalue  != NULL);

    newcount = popcopy->newfncount - popcopy->oldfncount;
    if(newcount <= 0)
    {
        goto Finished;
    }

    /* Allocate memory for newcount functions */
    fnvalue = (ocache_user_function *)OMALLOC(popcopy, sizeof(ocache_user_function) * newcount);
    if(fnvalue == NULL)
    {
        result = popcopy->oomcode;
        goto Finished;
    }

    /* Skip first oldfncount functions */
    zend_hash_internal_pointer_reset(CG(function_table));
    for(index = 0; index < popcopy->oldfncount; index++)
    {
        zend_hash_move_forward(CG(function_table));
    }

    for(index = 0; index < newcount; index++)
    {
        /* Copy function to the shared memory */
        zend_hash_get_current_key_ex(CG(function_table), &fname, &fnamelen, NULL, 0, NULL);
        zend_hash_get_current_data(CG(function_table), (void **)&ozendfn);

        nzendfn = NULL;
        result = copy_zend_function(popcopy, ozendfn, &nzendfn);
        if(FAILED(result))
        {
            goto Finished;
        }

        fnvalue[index].fentry = nzendfn;

        fnvalue[index].fname = OMALLOC(popcopy, fnamelen);
        if(fnvalue[index].fname == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        memcpy_s(fnvalue[index].fname, fnamelen, fname, fnamelen);
        fnvalue[index].fnamelen = fnamelen - 1;

        /* Move to next function to copy */
        zend_hash_move_forward(CG(function_table));
    }

    pvalue->functions = fnvalue;
    pvalue->fcount    = newcount;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_zend_functions", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(fnvalue != NULL)
        {
            OFREE(popcopy, fnvalue);
            fnvalue = NULL;
        }
    }

    dprintverbose("end copyin_zend_functions");

    return result;
}

static int copyin_zend_classes(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int          result   = NONFATAL;
    unsigned int newcount = 0;
    unsigned int index    = 0;
    char *       cname    = NULL;
    unsigned int cnamelen = 0;

    ocache_user_class *  clvalue   = NULL;
    zend_class_entry *   ozendcl   = NULL;
    zend_class_entry *   nzendcl   = NULL;

    dprintverbose("start copyin_zend_classes");

    _ASSERT(popcopy != NULL);
    _ASSERT(pvalue  != NULL);

    newcount = popcopy->newclcount - popcopy->oldclcount;
    if(newcount <= 0)
    {
        goto Finished;
    }

    /* Allocate memory in shared memory segment */
    clvalue = (ocache_user_class *)OMALLOC(popcopy, sizeof(ocache_user_class) * newcount);
    if(clvalue == NULL)
    {
        result = popcopy->oomcode;
        goto Finished;
    }

    /* Move the hash index forward by oldclcount */
    zend_hash_internal_pointer_reset(CG(class_table));
    for(index = 0; index < popcopy->oldclcount; index++)
    {
        zend_hash_move_forward(CG(class_table));
    }

    for(index = 0; index < newcount; index++)
    {
        /* Copy this class to shared memory */
        zend_hash_get_current_key_ex(CG(class_table), &cname, &cnamelen, NULL, 0, NULL);

        ozendcl = NULL;
        zend_hash_get_current_data(CG(class_table), (void **)&ozendcl);
        ozendcl = *((zend_class_entry **)ozendcl);

        clvalue[index].cname    = NULL;
        clvalue[index].cnamelen = 0;
        clvalue[index].pcname   = NULL;
        clvalue[index].centry   = NULL;

        nzendcl = NULL;
        result = copy_zend_class_entry(popcopy, ozendcl, &nzendcl);
        if(FAILED(result))
        {
            goto Finished;
        }

        clvalue[index].centry = nzendcl;

        clvalue[index].cname = OMALLOC(popcopy, cnamelen);
        if(clvalue[index].cname == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        clvalue[index].cnamelen = cnamelen - 1;
        memcpy_s(clvalue[index].cname, cnamelen, cname, cnamelen);

        if(ozendcl->parent != NULL)
        {
            clvalue[index].pcname = OSTRDUP(popcopy, ozendcl->parent->name);
            if (clvalue[index].pcname == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }
            zend_str_tolower(clvalue[index].pcname, strlen(clvalue[index].pcname));
        }
        else
        {
            clvalue[index].pcname = NULL;
        }

        /* Move class hashtable index forward */
        zend_hash_move_forward(CG(class_table));
    }

    pvalue->classes = clvalue;
    pvalue->ccount  = newcount;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_zend_classes", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(clvalue != NULL)
        {
            OFREE(popcopy, clvalue);
            clvalue = NULL;
        }
    }

    dprintverbose("end copyin_zend_classes");

    return result;
}

static int copyin_zend_constants(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int          result   = NONFATAL;
    unsigned int newcount = 0;
    unsigned int index    = 0;
    char *       tname    = NULL;
    unsigned int tnamelen = 0;

    zend_constant *         ozendt  = NULL;
    zend_constant *         nzendt  = NULL;
    ocache_user_constant *  tnvalue = NULL;

    dprintverbose("start copyin_zend_constants");

    _ASSERT(popcopy != NULL);
    _ASSERT(pvalue  != NULL);

    newcount = popcopy->newtncount - popcopy->oldtncount;
    if(newcount <= 0)
    {
        goto Finished;
    }

    /* Allocate memory for newcount constants */
    tnvalue = (ocache_user_constant *)OMALLOC(popcopy, sizeof(ocache_user_constant) * newcount);
    if(tnvalue == NULL)
    {
        result = popcopy->oomcode;
        goto Finished;
    }

    /* Skip first oldtncount constants */
    zend_hash_internal_pointer_reset(EG(zend_constants));
    for(index = 0; index < popcopy->oldtncount; index++)
    {
        zend_hash_move_forward(EG(zend_constants));
    }

    for(index = 0; index < newcount; index++)
    {
        /* Copy function to the shared memory */
        zend_hash_get_current_key_ex(EG(zend_constants), &tname, &tnamelen, NULL, 0, NULL);
        zend_hash_get_current_data(EG(zend_constants), (void **)&ozendt);

        nzendt = NULL;
        result = copy_zend_constant_entry(popcopy, ozendt, &nzendt);
        if(FAILED(result))
        {
            goto Finished;
        }

        tnvalue[index].tentry = nzendt;

        tnvalue[index].tname = OMALLOC(popcopy, tnamelen);
        if(tnvalue[index].tname == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        memcpy_s(tnvalue[index].tname, tnamelen, tname, tnamelen);
        tnvalue[index].tnamelen = tnamelen - 1;

        /* Move to next function to copy */
        zend_hash_move_forward(EG(zend_constants));
    }

    pvalue->constants = tnvalue;
    pvalue->tcount    = newcount;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_zend_constants", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(tnvalue != NULL)
        {
            OFREE(popcopy, tnvalue);
            tnvalue = NULL;
        }
    }

    dprintverbose("end copyin_zend_constants");

    return result;
}

static int copyin_messages(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int                   result   = NONFATAL;
    unsigned int          count    = 0;
    ocache_user_message * messages = NULL;
    ocache_user_message * pmessage = NULL;
    ocache_user_message * pnewmsg  = NULL;

    dprintverbose("start copyin_messages");

    _ASSERT(popcopy != NULL);
    _ASSERT(pvalue  != NULL);

    /* Allocate memory for zend_error messages */
    messages = (ocache_user_message *)OMALLOC(popcopy, sizeof(ocache_user_message) * zend_llist_count(WCG(errmsglist)));
    if(messages == NULL)
    {
        result = popcopy->oomcode;
        goto Finished;
    }

    pmessage = (ocache_user_message *)zend_llist_get_first(WCG(errmsglist));
    while(pmessage != NULL)
    {
        pnewmsg = &messages[count];

        pnewmsg->type   = pmessage->type;
        pnewmsg->lineno = pmessage->lineno;

        pnewmsg->filename = OSTRDUP(popcopy, pmessage->filename);
        if(pnewmsg->filename == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        pnewmsg->message = OSTRDUP(popcopy, pmessage->message);
        if(pnewmsg->message == NULL)
        {
            result = popcopy->oomcode;
            goto Finished;
        }

        count++;
        pmessage = (ocache_user_message *)zend_llist_get_next(WCG(errmsglist));
    }

    pvalue->messages = messages;
    pvalue->mcount   = count;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_messages", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(messages != NULL)
        {
            OFREE(popcopy, messages);
            messages = NULL;
        }
    }

    dprintverbose("end copyin_messages");

    return result;
}

static int copyin_auto_globals(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int                   result   = NONFATAL;
    unsigned int          index    = 0;
    unsigned int          hcount   = 0;
    unsigned int          count    = 0;
    unsigned int          aindex   = 0;
    char *                aname    = NULL;
    unsigned int          anamelen = 0;
    ocache_user_aglobal * aglobals = NULL;
    ocache_user_aglobal * pnewag   = NULL;
    zend_auto_global *    pzenda   = NULL;

    dprintverbose("start copyin_auto_globals");

    _ASSERT(popcopy != NULL);
    _ASSERT(pvalue  != NULL);

    hcount = zend_hash_num_elements(CG(auto_globals));

    /* Get count of auto globals which are active */
    zend_hash_internal_pointer_reset(CG(auto_globals));
    for(index = 0; index < hcount; index++)
    {
        zend_hash_get_current_data(CG(auto_globals), &pzenda);
        if(!pzenda->armed)
        {
            count++;
        }

        zend_hash_move_forward(CG(auto_globals));
    }

    if(count <= 0)
    {
        goto Finished;
    }

    /* Allocate memory for active auto globals */
    aglobals = (ocache_user_aglobal *)OMALLOC(popcopy, sizeof(ocache_user_aglobal) * count);
    if(aglobals == NULL)
    {
        result = popcopy->oomcode;
        goto Finished;
    }

    /* Store the list of active auto globals */
    zend_hash_internal_pointer_reset(CG(auto_globals));
    for(index = 0; index < hcount; index++)
    {
        zend_hash_get_current_key_ex(CG(auto_globals), &aname, &anamelen, NULL, 0, NULL);
        zend_hash_get_current_data(CG(auto_globals), &pzenda);

        if(!pzenda->armed)
        {
            pnewag = &aglobals[aindex];

            pnewag->aname = OSTRDUP(popcopy, aname);
            if(pnewag->aname == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            pnewag->anamelen = anamelen - 1;
            aindex++;
        }

        zend_hash_move_forward(CG(auto_globals));
    }

    pvalue->aglobals = aglobals;
    pvalue->acount   = count;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyin_auto_globals", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(aglobals != NULL)
        {
            OFREE(popcopy, aglobals);
            aglobals = NULL;
        }
    }

    dprintverbose("end copyin_auto_globals");

    return result;
}

int opcopy_zend_copyin(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int result = NONFATAL;

    dprintverbose("start opcopy_zend_copyin");

    _ASSERT(popcopy != NULL);
    _ASSERT(pvalue  != NULL);

    _ASSERT(popcopy->fnmalloc  != NULL);
    _ASSERT(popcopy->fnrealloc != NULL);
    _ASSERT(popcopy->fnstrdup  != NULL);
    _ASSERT(popcopy->fnfree    != NULL);

    /* Copy main opcode array */
    result = copyin_zend_op_array(popcopy, pvalue TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(pvalue->oparray != NULL);

    /* Copy zend functions */
    result = copyin_zend_functions(popcopy, pvalue TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(pvalue->fcount == 0 || pvalue->functions != NULL);

    /* Copy zend classes */
    result = copyin_zend_classes(popcopy, pvalue TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(pvalue->ccount == 0 || pvalue->classes != NULL);

    /* Copy zend constants */
    result = copyin_zend_constants(popcopy, pvalue TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(pvalue->tcount == 0 || pvalue->constants != NULL);

    /* Copy warning/error messages */
    if(WCG(errmsglist) != NULL)
    {
        result = copyin_messages(popcopy, pvalue TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    if(PG(auto_globals_jit))
    {
        result = copyin_auto_globals(popcopy, pvalue TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    _ASSERT(pvalue->acount == 0 || pvalue->aglobals != NULL);

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in opcopy_zend_copyin", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end opcopy_zend_copyin");

    return result;
}

static int copyout_zend_op_array(opcopy_context * popcopy, zend_op_array * poldopa, zend_op_array ** ppnewopa)
{
    int             result    = NONFATAL;
    int             allocated = 0;

    dprintverbose("start copyout_zend_op_array");

    _ASSERT(popcopy  != NULL);
    _ASSERT(poldopa  != NULL);
    _ASSERT(ppnewopa != NULL);

    result = copy_zend_op_array(popcopy, poldopa, ppnewopa);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyout_zend_op_array", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end copyout_zend_op_array");

    return result;
}

static int copyout_zend_function(opcopy_context * popcopy, zend_function * poldf, zend_function ** ppnewf)
{
    int result = NONFATAL;
    dprintverbose("start copyout_zend_function");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldf   != NULL);
    _ASSERT(ppnewf  != NULL);

    result = copy_zend_function(popcopy, poldf, ppnewf);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyout_zend_function", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end copyout_zend_function");

    return result;
}

static int copyout_zend_class(opcopy_context * popcopy, zend_class_entry * poldce, zend_class_entry ** ppnewce)
{
    int result = NONFATAL;
    dprintverbose("start copyout_zend_class");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldce  != NULL);
    _ASSERT(ppnewce != NULL);

    result = copy_zend_class_entry(popcopy, poldce, ppnewce);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyout_zend_class", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end copyout_zend_class");

    return result;
}

static int copyout_zend_constant(opcopy_context * popcopy, zend_constant * poldt, zend_constant ** ppnewt)
{
    int result = NONFATAL;
    dprintverbose("start copyout_zend_constant");

    _ASSERT(popcopy != NULL);
    _ASSERT(poldt   != NULL);
    _ASSERT(ppnewt  != NULL);

    result = copy_zend_constant_entry(popcopy, poldt, ppnewt);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in copyout_zend_constant", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end copyout_zend_constant");

    return result;
}


int opcopy_zend_copyout(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC)
{
    int   result = NONFATAL;
    int   count  = 0;
    int   index  = 0;
    int   status = SUCCESS;

    zend_op_array *         poparray  = NULL;
    ocache_user_function *  pfunction = NULL;
    zend_function *         pzendf    = NULL;
    ocache_user_constant *  pconstant = NULL;
    zend_constant *         pzendt    = NULL;
    ocache_user_class *     pclass    = NULL;
    zend_class_entry **     ppzendc   = NULL;
    zend_class_entry *      pzendc    = NULL;
    ocache_user_message *   pmessage  = NULL;
    va_list                 msgargs   = {0};
    ocache_user_aglobal *   paglobal  = NULL;

    dprintverbose("start opcopy_zend_copyout");

    _ASSERT(popcopy           != NULL);
    _ASSERT(pvalue            != NULL);
    _ASSERT(popcopy->cacheopa == NULL);

    /* Copy zend_class_entries */
    if(pvalue->classes != NULL)
    {
        count = pvalue->ccount;
        for(index = 0; index < count; index++)
        {
            pclass = &pvalue->classes[index];
            pzendc = NULL;

            _ASSERT(pclass != NULL);

            /* If class is already there in class table, skip */
            if(pclass->cnamelen > 0 && zend_hash_exists(CG(class_table), pclass->cname, pclass->cnamelen + 1))
            {
                continue;
            }

            result = copyout_zend_class(popcopy, pclass->centry, &pzendc);
            if(FAILED(result))
            {
                goto Finished;
            }

            _ASSERT(pzendc != NULL);

            /* Setup inheritance again by setting parent pointer */
            if(pclass->pcname != NULL)
            {
                ppzendc = NULL;
#ifdef ZEND_ENGINE_2_4
                if(zend_lookup_class_ex(pclass->pcname, strlen(pclass->pcname), NULL, 0, &ppzendc TSRMLS_CC) == FAILURE)
#else /* ZEND_ENGINE_2_3 and below */
                if(zend_lookup_class_ex(pclass->pcname, strlen(pclass->pcname), 0, &ppzendc TSRMLS_CC) == FAILURE)
#endif /* ZEND_ENGINE_2_4 */
                {
                    for(index = index - 1; index >= 0; index--)
                    {
                        pclass = &pvalue->classes[index];
                        zend_hash_del(EG(class_table), pclass->cname, pclass->cnamelen + 1);
                    }

                    result = WARNING_OPCOPY_MISSING_PARENT;
                    goto Finished;
                }
                else
                {
                    pzendc->parent = *ppzendc;
                    zend_do_inheritance(pzendc, *ppzendc TSRMLS_CC);
                }
            }

            ppzendc = (zend_class_entry **)OMALLOC(popcopy, sizeof(zend_class_entry *));
            if(ppzendc == NULL)
            {
                result = popcopy->oomcode;
                goto Finished;
            }

            *ppzendc = pzendc;
            zend_hash_add(EG(class_table), pclass->cname, pclass->cnamelen + 1, ppzendc, sizeof(zend_class_entry *), NULL);

            /*OFREE(popcopy, pzendc); */
        }
    }

    /* Copy the opcode array */
    if(pvalue->oparray != NULL)
    {
        result = copyout_zend_op_array(popcopy, pvalue->oparray, &poparray);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    /* Copy constants which are declared in the file */
    if(pvalue->constants != NULL)
    {
        count = pvalue->tcount;
        for(index = 0; index < count; index++)
        {
            pconstant = &pvalue->constants[index];
            _ASSERT(pconstant != NULL);

            pzendt = NULL;
            result = copyout_zend_constant(popcopy, pconstant->tentry, &pzendt);
            if(FAILED(result))
            {
                goto Finished;
            }

            _ASSERT(pzendt != NULL);

            zend_hash_add(EG(zend_constants), pconstant->tname, pconstant->tnamelen + 1, pzendt, sizeof(zend_constant), NULL);
            OFREE(popcopy, pzendt);
        }
    }

    /* Copy functions */
    if(pvalue->functions != NULL)
    {
        count = pvalue->fcount;
        for(index = 0; index < count; index++)
        {
            pfunction = &pvalue->functions[index];
            _ASSERT(pfunction != NULL);

            pzendf = NULL;
            result = copyout_zend_function(popcopy, pfunction->fentry, &pzendf);
            if(FAILED(result))
            {
                goto Finished;
            }

            _ASSERT(pzendf != NULL);

            zend_hash_add(EG(function_table), pfunction->fname, pfunction->fnamelen + 1, pzendf, sizeof(zend_function), NULL);
            OFREE(popcopy, pzendf);
        }
    }

    /* Generate messages generated when PHP core compiled it */
    if(pvalue->messages != NULL)
    {
        count = pvalue->mcount;
        for(index = 0; index < count; index++)
        {
            pmessage = &pvalue->messages[index];
            _ASSERT(pmessage != NULL);

            original_error_cb(pmessage->type, pmessage->filename, pmessage->lineno, pmessage->message, msgargs);
        }
    }

    if(PG(auto_globals_jit) && pvalue->aglobals != NULL)
    {
        count = pvalue->acount;
        for(index = 0; index < count; index++)
        {
            paglobal = &pvalue->aglobals[index];
            _ASSERT(paglobal != NULL);

            zend_is_auto_global(paglobal->aname, paglobal->anamelen TSRMLS_CC);
        }
    }

    /* Save the oparray in opcopy_context */
    popcopy->cacheopa = poparray;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in opcopy_zend_copyout", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end opcopy_zend_copyout");

    return result;
}

void opcopy_runtest()
{
    int              result  = NONFATAL;
    opcopy_context * popcopy = NULL;

    dprintverbose("*** STARTING OPCOPY TESTS ***");

    result = opcopy_create(&popcopy);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Test copy zval, znode */

    /* Test copy arg_info, arg_info_array, property_info */

    /* Test copy zend_function */

    /* Test copy Hashtable */

    /* Test zend_op and zend_op_array */

    /* Test copy zend_class_entry */

    _ASSERT(SUCCEEDED(result));

Finished:

    if(popcopy != NULL)
    {
        opcopy_terminate(popcopy);
        opcopy_destroy(popcopy);

        popcopy =  NULL;
    }

    dprintverbose("*** ENDING OPCOPY TESTS ***");

    return;
}

/*
 * Trait Alias & Trait Precedence stuff
 */

#ifdef ZEND_ENGINE_2_4

#define OPCOPY_TRAIT_METHOD(dst, src) \
    CHECK(dst = \
         (zend_trait_method_reference *) OMALLOC(popcopy, sizeof(zend_trait_method_reference))); \
    memcpy(dst, src, sizeof(zend_trait_method_reference)); \
\
    if (src->method_name) { \
        CHECK((dst->method_name = OSTRDUP(popcopy, src->method_name))); \
        dst->mname_len = src->mname_len; \
    } \
 \
    if (src->class_name) { \
        CHECK((dst->class_name = OSTRDUP(popcopy, src->class_name))); \
        dst->cname_len = src->cname_len; \
    } \
    if (src->ce) { \
        dst->ce = NULL; \
        copy_zend_class_entry(popcopy, src->ce, &dst->ce); \
        CHECK(dst->ce); \
    }

void free_zend_trait_method(opcopy_context * popcopy, zend_trait_method_reference *src)
{
    if (src)
    {
        if (src->method_name) {
            OFREE(popcopy, ((void *)src->method_name));
            src->method_name = NULL;
        }
        if (src->class_name) {
            OFREE(popcopy, ((void *)src->class_name));
            src->class_name = NULL;
        }
        if (src->ce) {
            free_zend_class_entry(popcopy, src->ce, DO_FREE);
            src->ce = NULL;
        }
    }
}

/* {{{ opcopy_trait_alias */
zend_trait_alias* opcopy_trait_alias(opcopy_context * popcopy, zend_trait_alias *src)
{
    int               result    = NONFATAL;
    zend_trait_alias *dst       = NULL;

    CHECK(dst = (zend_trait_alias *) OMALLOC(popcopy, sizeof(zend_trait_alias)));
    memcpy(dst, src, sizeof(zend_trait_alias));

    if (src->alias) {
        CHECK((dst->alias = OSTRDUP(popcopy, src->alias)));
        dst->alias_len = src->alias_len;
    }

#ifndef ZEND_ENGINE_2_5
    if (src->function) {
        /* Always copy the function in this case */
        dst->function = NULL;
        copy_zend_function(popcopy, src->function, &dst->function);
        CHECK(dst->function);
    }
#endif

    OPCOPY_TRAIT_METHOD(dst->trait_method, src->trait_method);

Finished:
    /* If failure, we need to free up the partial stuff we alloc'd. */
    if (FAILED(result))
    {
        dprintimportant("failure %d in opcopy_trait_alias", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if (dst != NULL)
        {
            if (dst->alias != NULL)
            {
                OFREE(popcopy, ((void *)dst->alias));
                dst->alias  = NULL;
            }
#ifndef ZEND_ENGINE_2_5
            if (dst->function != NULL)
            {
                free_zend_function(popcopy, dst->function, DO_FREE);
                dst->function = NULL;
            }
#endif
            if (dst->trait_method != NULL)
            {
                free_zend_trait_method(popcopy, dst->trait_method);
                dst->trait_method = NULL;
            }
            OFREE(popcopy, dst);
            dst = NULL;
        }
    }

    return dst;
}
/* }}} */

/* {{{ opcopy_trait_precedence */
zend_trait_precedence* opcopy_trait_precedence(opcopy_context * popcopy, zend_trait_precedence *src)
{
    int                    result    = NONFATAL;
    zend_trait_precedence *dst       = NULL;

    CHECK(dst = (zend_trait_precedence *) OMALLOC(popcopy, sizeof(zend_trait_precedence)));
    memcpy(dst, src, sizeof(zend_trait_precedence));

#ifndef ZEND_ENGINE_2_5
    if (src->function) {
        /* Always copy the function in this case */
        dst->function = NULL;
        result = copy_zend_function(popcopy, src->function, &dst->function);
        if (FAILED(result))
        {
            goto Finished;
        }
    }
#endif

    if (src->exclude_from_classes && *src->exclude_from_classes) {
        int i = 0, num_classes = 0;

        while (src->exclude_from_classes[num_classes]) { num_classes++; }

        CHECK(dst->exclude_from_classes =
            (zend_class_entry **) OMALLOC(popcopy, sizeof(zend_class_entry *)*(num_classes+1)));

        while (src->exclude_from_classes[i] && i < num_classes) {
            char *name = (char *) src->exclude_from_classes[i];

            CHECK(dst->exclude_from_classes[i] = (zend_class_entry *) OSTRDUP(popcopy, name));
            i++;
        }
        dst->exclude_from_classes[i] = NULL;
    }

    OPCOPY_TRAIT_METHOD(dst->trait_method, src->trait_method);

Finished:
    /* If failure, we need to free up the partial stuff we alloc'd. */
    if (FAILED(result))
    {
        if (dst != NULL)
        {
#ifndef ZEND_ENGINE_2_5
            if (dst->function != NULL) {
                free_zend_function(popcopy, dst->function, DO_FREE);
                dst->function = NULL;
            }
#endif
            if (dst->exclude_from_classes != NULL) {
                int i = 0;
                while (dst->exclude_from_classes[i] != NULL) {
                    OFREE(popcopy, dst->exclude_from_classes[i]);
                    i++;
                }
            }
            if (dst->trait_method != NULL)
            {
                free_zend_trait_method(popcopy, dst->trait_method);
                dst->trait_method = NULL;
            }
            OFREE(popcopy, dst);
            dst = NULL;
        }
    }
    return dst;
}
/* }}} */

#endif

