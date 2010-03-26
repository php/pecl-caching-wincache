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
   | Module: wincache_ocache.c                                                                    |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

/* Globals */
unsigned int gocacheid = 1;

/* Public methods */
int ocache_create(ocache_context ** ppcache)
{
    int              result = NONFATAL;
    ocache_context * pcache = NULL;

    dprintverbose("start ocache_create");

    _ASSERT(ppcache != NULL);
    *ppcache = NULL;

    /* Allocate memory for ocache_context */
    pcache = (ocache_context *)alloc_pemalloc(sizeof(ocache_context));
    if(pcache == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pcache->id        = gocacheid++;
    pcache->islocal   = 0;
    pcache->hinitdone = NULL;
    pcache->resnumber = -1;
    pcache->memaddr   = NULL;
    pcache->header    = NULL;
    pcache->pfilemap  = NULL;
    pcache->prwlock   = NULL;
    pcache->palloc    = NULL;

    *ppcache = pcache;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in ocache_create", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end ocache_create");

    return result;
}

void ocache_destroy(ocache_context * pcache)
{
    dprintverbose("start ocache_destroy");

    if(pcache != NULL)
    {
        alloc_pefree(pcache);
        pcache = NULL;
    }

    dprintverbose("end ocache_destroy");

    return;
}

int ocache_initialize(ocache_context * pcache, unsigned short islocal, int resnumber, unsigned int cachesize TSRMLS_DC)
{
    int             result   = NONFATAL;
    size_t          size     = 0;
    ocache_header * header   = NULL;
    unsigned short  mapclass = FILEMAP_MAP_SFIXED;
    unsigned short  locktype = LOCK_TYPE_SHARED;
    unsigned char   isfirst  = 1;
    char            evtname[   MAX_PATH];

    dprintverbose("start ocache_initialize");

    _ASSERT(pcache    != NULL);
    _ASSERT(resnumber != -1);
    _ASSERT(cachesize >= OCACHE_SIZE_MINIMUM && cachesize <= OCACHE_SIZE_MAXIMUM);

    /* Initialize memory map to store opcodes */
    result = filemap_create(&pcache->pfilemap);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(islocal)
    {
        mapclass = FILEMAP_MAP_LRANDOM;
        locktype = LOCK_TYPE_LOCAL;

        pcache->islocal = islocal;
    }

    /* shmfilepath = NULL */
    result = filemap_initialize(pcache->pfilemap, FILEMAP_TYPE_BYTECODES, mapclass, cachesize, NULL TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    pcache->memaddr = (char *)pcache->pfilemap->mapaddr;
    size = filemap_getsize(pcache->pfilemap TSRMLS_CC);

    /* Create allocator for opcode memory map */
    result = alloc_create(&pcache->palloc);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* initmemory = 1 for all page file backed shared memory allocators */
    result = alloc_initialize(pcache->palloc, islocal, "BYTECODE_SEGMENT", pcache->memaddr, size, 1 TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Get memory for cache header */
    pcache->header = (ocache_header *)alloc_get_cacheheader(pcache->palloc, sizeof(ocache_header), CACHE_TYPE_BYTECODES);
    if(pcache->header == NULL)
    {
        result = FATAL_OCACHE_INITIALIZE;
        goto Finished;
    }

    header = pcache->header;

    /* Create xread xwrite lock for the cache */
    result = lock_create(&pcache->prwlock);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = lock_initialize(pcache->prwlock, "BYTECODE_CACHE", 0, locktype, LOCK_USET_XREAD_XWRITE, NULL TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = lock_getnewname(pcache->prwlock, "OCACHE_INIT", evtname, MAX_PATH);
    if(FAILED(result))
    {
        goto Finished;
    }

    pcache->hinitdone = CreateEvent(NULL, TRUE, FALSE, evtname);
    if(pcache->hinitdone == NULL)
    {
        result = FATAL_OCACHE_INIT_EVENT;
        goto Finished;
    }

    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        _ASSERT(islocal == 0);
        isfirst = 0;

        /* Wait for other process to initialize completely */
        WaitForSingleObject(pcache->hinitdone, INFINITE);
    }

    if(islocal || isfirst)
    {
        lock_writelock(pcache->prwlock);
    
        header->mapcount    = 1;
        header->itemcount   = 0;
        header->hitcount    = 0;
        header->misscount   = 0;

        SetEvent(pcache->hinitdone);

        lock_writeunlock(pcache->prwlock);
    }
    else
    {
        InterlockedIncrement(&header->mapcount);
    }

    pcache->resnumber = resnumber;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in ocache_initialize", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pcache->pfilemap != NULL)
        {
            filemap_terminate(pcache->pfilemap);
            filemap_destroy(pcache->pfilemap);

            pcache->pfilemap = NULL;
        }

        if(pcache->palloc != NULL)
        {
            alloc_terminate(pcache->palloc);
            alloc_destroy(pcache->palloc);

            pcache->palloc = NULL;
        }

        if(pcache->prwlock != NULL)
        {
            lock_terminate(pcache->prwlock);
            lock_destroy(pcache->prwlock);

            pcache->prwlock = NULL;
        }

        if(pcache->hinitdone != NULL)
        {
            CloseHandle(pcache->hinitdone);
            pcache->hinitdone = NULL;
        }
    }

    dprintverbose("end ocache_initialize");

    return result;
}

void ocache_terminate(ocache_context * pcache)
{
    dprintverbose("start ocache_terminate");

    if(pcache != NULL)
    {
        if(pcache->header != NULL)
        {
            InterlockedDecrement(&pcache->header->mapcount);
            pcache->header = NULL;
        }

        if(pcache->palloc != NULL)
        {
            alloc_terminate(pcache->palloc);
            alloc_destroy(pcache->palloc);

            pcache->palloc = NULL;
        }

        if(pcache->pfilemap != NULL)
        {
            filemap_terminate(pcache->pfilemap);
            filemap_destroy(pcache->pfilemap);

            pcache->pfilemap = NULL;
        }

        if(pcache->prwlock != NULL)
        {
            lock_terminate(pcache->prwlock);
            lock_destroy(pcache->prwlock);

            pcache->prwlock = NULL;
        }

        if(pcache->hinitdone != NULL)
        {
            CloseHandle(pcache->hinitdone);
            pcache->hinitdone = NULL;
        }
    }

    dprintverbose("end ocache_terminate");

    return;
}

int ocache_createval(ocache_context * pcache, const char * filename, zend_file_handle * file_handle, int type, zend_op_array ** poparray, ocache_value ** ppvalue TSRMLS_DC)
{
    int              result  = NONFATAL;
    opcopy_context * popcopy = NULL;
    ocache_value *   pvalue  = NULL;
    size_t           hoffset = 0;
    zend_op_array *  oparray = NULL;

    unsigned int     oldfnc  = 0;
    unsigned int     oldclc  = 0;
    unsigned int     oldtnc  = 0;

    dprintverbose("start ocache_createval");

    _ASSERT(pcache      != NULL);
    _ASSERT(filename    != NULL);
    _ASSERT(file_handle != NULL);
    _ASSERT(poparray    != NULL);
    _ASSERT(ppvalue     != NULL);

    *ppvalue = NULL;

    /* Set the old function count and class count in opcopy_context */
    oldfnc = zend_hash_num_elements(CG(function_table));
    oldclc = zend_hash_num_elements(CG(class_table));
    oldtnc = zend_hash_num_elements(EG(zend_constants));

    /* set zend_error_cb to collect messages generated at compile time */
    /* call function which will produce opcodes by parsing php code */
    /* and set zend_error_cb back to original valie */
    zend_error_cb = wincache_error_cb;

    /* Call to original_compile_file might trigger a bailout */
    /* Catch the bailout, do cleanup and retrigger bailout */
    zend_try
    {
        oparray = original_compile_file(file_handle, type TSRMLS_CC);
    }
    zend_catch
    {
        result  = FATAL_ZEND_BAILOUT;
    }
    zend_end_try();

    if(FAILED(result))
    {
        goto Finished;
    }

    /* Revert back to original error handler */
    zend_error_cb = original_error_cb;

    if(oparray == NULL)
    {
        result = FATAL_OCACHE_ORIGINAL_COMPILE;
        goto Finished;
    }

    *poparray = oparray;

    /* Create opcopy_context only if original_compile_file returned */
    result = opcopy_create(&popcopy);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Create memory pool for this value */
    result = alloc_create_mpool(pcache->palloc, &hoffset);
    if(FAILED(result))
    {
        goto Finished;
    }
    
    /* Copy oparray to cache */
    result = opcopy_initialize(popcopy, pcache->palloc, hoffset, pcache->resnumber);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Set old function count, class count and constant count in opcopy_context */
    popcopy->oldfncount = oldfnc;
    popcopy->oldclcount = oldclc;
    popcopy->oldtncount = oldtnc;

    /* Get the new function_count and class_count after compile */
    popcopy->newfncount = zend_hash_num_elements(CG(function_table));
    popcopy->newclcount = zend_hash_num_elements(CG(class_table));
    popcopy->newtncount = zend_hash_num_elements(EG(zend_constants));

    popcopy->oparray    = oparray;
    popcopy->optype     = OPCOPY_OPERATION_COPYIN;

    pvalue = (ocache_value *)alloc_smalloc(pcache->palloc, sizeof(ocache_value));
    if(pvalue == NULL)
    {
        result = FATAL_OUT_OF_SMEMORY;
        goto Finished;
    }

    pvalue->hoffset    = hoffset;
    pvalue->oparray    = NULL;
    pvalue->functions  = NULL;
    pvalue->fcount     = 0;
    pvalue->classes    = NULL;
    pvalue->ccount     = 0;
    pvalue->constants  = NULL;
    pvalue->tcount     = 0;

    pvalue->messages   = NULL;
    pvalue->mcount     = 0;
    pvalue->aglobals   = NULL;
    pvalue->acount     = 0;
    pvalue->is_deleted = 0;
    pvalue->hitcount   = 0;
    pvalue->refcount   = 0;

    HANDLE_BLOCK_INTERRUPTIONS();

    /* Store oparray, new functions and new classes cache */
    result = opcopy_zend_copyin(popcopy, pvalue TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    HANDLE_UNBLOCK_INTERRUPTIONS();
    
    InterlockedIncrement(&pcache->header->itemcount);
    InterlockedIncrement(&pcache->header->misscount);

    *ppvalue = pvalue;

    _ASSERT(SUCCEEDED(result));

Finished:

    /* Destroy messages which were collected during original_compile */
    if(WCG(errmsglist) != NULL)
    {
        zend_llist_destroy(WCG(errmsglist));
        alloc_efree(WCG(errmsglist));
        WCG(errmsglist) = NULL;
    }

    /* After cleanup, do an official bailout */
    if(result == FATAL_ZEND_BAILOUT)
    {
        zend_error_cb = original_error_cb;
        result = NONFATAL;

        zend_bailout();
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in ocache_createval", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pvalue != NULL)
        {
            if(pvalue->hoffset != 0)
            {
                alloc_free_mpool(pcache->palloc, pvalue->hoffset);
                pvalue->hoffset = 0;
            }

            alloc_sfree(pcache->palloc, pvalue);
            pvalue = NULL;
        }

        if(poparray != NULL)
        {
            /* oparray is from original compile. We can use */
            /* the opcodes. Set result to NONFATAL */
            result = NONFATAL;
        }
    }

    if(popcopy != NULL)
    {
        opcopy_terminate(popcopy);
        opcopy_destroy(popcopy);

        popcopy = NULL;
    }

    dprintverbose("end ocache_createval");

    return result;
}

void ocache_destroyval(ocache_context * pocache, ocache_value * pvalue)
{
    dprintverbose("start ocache_destroyval");

    _ASSERT(pvalue->refcount == 0);

    if(pvalue != NULL)
    {
        if(pvalue->hoffset != 0)
        {
            alloc_free_mpool(pocache->palloc, pvalue->hoffset);
            pvalue->hoffset = 0;
        }
        
        alloc_sfree(pocache->palloc, pvalue);
        pvalue = NULL;

        InterlockedDecrement(&pocache->header->itemcount);
    }

    dprintverbose("end ocache_destroyval");

    return;
}

int ocache_useval(ocache_context * pcache, ocache_value * pvalue, zend_op_array ** pparray TSRMLS_DC)
{
    int                 result   = NONFATAL;
    opcopy_context *    popcopy  = NULL;
    unsigned int        hcount   = 9999;
    unsigned int        index    = 0;
    zend_op_array *     oparray  = NULL;
    zend_execute_data * execdata = NULL;

    dprintverbose("start ocache_useval");

    _ASSERT(pcache      != NULL);
    _ASSERT(pvalue      != NULL);
    _ASSERT(pparray     != NULL);

    *pparray = NULL;

    result = opcopy_create(&popcopy);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Make refcounts large so that caller don't */
    /* end up destroying the cached data */
    if(pvalue->oparray != NULL)
    {
        *(pvalue->oparray->refcount) = hcount;
    }

    if(pvalue->functions != NULL)
    {
        for(index = 0; index < pvalue->fcount; index++)
        {
            *(pvalue->functions[index].fentry->op_array.refcount) = hcount;
        }
    }

    if(pvalue->classes != NULL)
    {
        for(index = 0; index < pvalue->ccount; index++)
        {
            pvalue->classes[index].centry->refcount = hcount;
        }
    }

    /* Get zend_op_array from cache and return that */
    /* Also add zend_functions and zend_classes from */
    /* cache to CG(function_table) and CG(class_table) */
    result = opcopy_initialize(popcopy, NULL, 0, pcache->resnumber);
    if(FAILED(result))
    {
        goto Finished;
    }

    popcopy->optype = OPCOPY_OPERATION_COPYOUT;
    popcopy->cacheopa = NULL;

    /* copyout might trigger a bailout which needs to be ignored */
    execdata = EG(current_execute_data);
    zend_try
    {
        result = opcopy_zend_copyout(popcopy, pvalue TSRMLS_CC);
    }
    zend_catch
    {
        CG(unclean_shutdown) = 0;
        EG(current_execute_data) = execdata;
        EG(in_execution) = 1;

        result = FATAL_ZEND_BAILOUT;
    }
    zend_end_try();

    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(popcopy->cacheopa != NULL);
    oparray = popcopy->cacheopa;

    *pparray = oparray;

Finished:

    if(popcopy != NULL)
    {
        opcopy_terminate(popcopy);
        opcopy_destroy(popcopy);

        popcopy = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in ocache_useval", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end ocache_useval");

    return result;
}

ocache_value * ocache_getvalue(ocache_context * pocache, size_t offset)
{
    _ASSERT(pocache != NULL);
    return ((offset != 0) ? ((ocache_value *)(pocache->memaddr + offset)) : NULL);
}

size_t ocache_getoffset(ocache_context * pocache, ocache_value * pvalue)
{
    _ASSERT(pocache != NULL);
    return ((pvalue != NULL) ? ((char *)pvalue - pocache->memaddr) : 0);
}

void ocache_refinc(ocache_context * pocache, ocache_value * pvalue)
{
    _ASSERT(pvalue != NULL);

    InterlockedIncrement(&pvalue->refcount);
    InterlockedIncrement(&pvalue->hitcount);
    InterlockedIncrement(&pocache->header->hitcount);

    return;
}

void ocache_refdec(ocache_context * pocache, ocache_value * pvalue)
{
    _ASSERT(pvalue != NULL);

    InterlockedDecrement(&pvalue->refcount);

    if(InterlockedCompareExchange(&pvalue->is_deleted, 1, 1) == 1 &&
       InterlockedCompareExchange(&pvalue->refcount, 0, 0) == 0)
    {
        ocache_destroyval(pocache, pvalue);
        pvalue = NULL;
    }

    return;
}

int ocache_getinfo(ocache_value * pvalue, ocache_entry_info ** ppinfo)
{
    int                 result = NONFATAL;
    ocache_entry_info * pinfo  = NULL;

    dprintverbose("start ocache_getinfo");

    _ASSERT(pvalue != NULL);
    _ASSERT(ppinfo != NULL);

    *ppinfo = NULL;
    
    pinfo = (ocache_entry_info *)alloc_emalloc(sizeof(ocache_entry_info));
    if(pinfo == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pinfo->fcount   = pvalue->fcount;
    pinfo->ccount   = pvalue->ccount;
    pinfo->hitcount = pvalue->hitcount;

    *ppinfo = pinfo;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in ocache_getinfo", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end ocache_getinfo");

    return result;
}

void ocache_freeinfo(ocache_entry_info * pinfo)
{
    if(pinfo != NULL)
    {
        alloc_efree(pinfo);
        pinfo = NULL;
    }

    return;
}

void ocache_runtest()
{
    int              result    = NONFATAL;
    ocache_context * pcache    = NULL;
    ocache_value *   pvalue    = NULL;
    unsigned char    islocal   = 0;
    int              resnum    = 0;
    unsigned int     cachesize = 32;
    char *           filename  = "testfile.php";

    TSRMLS_FETCH();
    dprintverbose("*** STARTING OCACHE TESTS ***");

    result = ocache_create(&pcache);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = ocache_initialize(pcache, islocal, resnum, cachesize TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(pcache->header   != NULL);
    _ASSERT(pcache->pfilemap != NULL);
    _ASSERT(pcache->prwlock  != NULL);

    _ASSERT(pcache->header->itemcount  == 0);
    _ASSERT(pcache->header->hitcount   == 0);
    _ASSERT(pcache->header->misscount  == 0);

    _ASSERT(SUCCEEDED(result));

Finished:

    if(pcache != NULL)
    {
        ocache_terminate(pcache);
        ocache_destroy(pcache);

        pcache =  NULL;
    }

    dprintverbose("*** ENDING OCACHE TESTS ***");

    return;
}

