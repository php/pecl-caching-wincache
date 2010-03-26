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
   | Module: wincache_fcnotify.c                                                                  |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

#define FCNOTIFY_VALUE(p, o)         ((fcnotify_value *)alloc_get_cachevalue(p, o))

static unsigned int WINAPI change_notification_thread(void * parg);
static void WINAPI process_change_notification(unsigned int status, unsigned int completion, OVERLAPPED * poverlapped);

static unsigned int WINAPI change_notification_thread(void * parg)
{
    return 0;
}

static void WINAPI process_change_notification(unsigned int status, unsigned int completion, OVERLAPPED * poverlapped)
{
    return;
}

static int findfolder_in_cache(fcnotify_context * pnotify, const char * folderpath, unsigned int index, fcnotify_value ** ppvalue)
{
    int               result  = NONFATAL;
    fcnotify_header * pheader = NULL;
    fcnotify_value *  pvalue  = NULL;

    dprintverbose("start findfolder_in_cache");

    _ASSERT(pnotify    != NULL);
    _ASSERT(folderpath != NULL);
    _ASSERT(ppvalue    != NULL);

    *ppvalue = NULL;

    pheader = pnotify->fcheader;
    pvalue = FCNOTIFY_VALUE(pnotify->fcalloc, pheader->values[index]);

    while(pvalue != NULL)
    {
        if(!_stricmp(pnotify->fcmemaddr + pvalue->folder_path, folderpath))
        {
            *ppvalue = pvalue;
            break;
        }
    }

    dprintverbose("end findfolder_in_cache");

    return result;
}

static int create_fcnotify_data(fcnotify_context * pnotify, const char * folderpath, fcnotify_value ** ppvalue)
{
    int              result  = NONFATAL;
    fcnotify_value * pvalue  = NULL;
    alloc_context *  palloc  = NULL;
    unsigned int     pathlen = 0;
    char *           paddr   = NULL;

    dprintverbose("start create_fcnotify_data");

    _ASSERT(pnotify    != NULL);
    _ASSERT(folderpath != NULL);
    _ASSERT(ppvalue    != NULL);

    *ppvalue = NULL;

    palloc = pnotify->fcalloc;
    pathlen = strlen(folderpath);

    pvalue = (fcnotify_value *)alloc_smalloc(palloc, sizeof(fcnotify_value) + ALIGNDWORD(pathlen + 1));
    if(pvalue == NULL)
    {
        result = FATAL_OUT_OF_SMEMORY;
        goto Finished;
    }

    pvalue->folder_path   = 0;
    pvalue->owner_pid     = pnotify->processid;
    pvalue->folder_handle = NULL;
    pvalue->prev_value    = 0;
    pvalue->next_value    = 0;

    paddr = (char *)(pvalue + 1);
    memcpy_s(pvalue + 1, pathlen, folderpath, pathlen);
    *(paddr + pathlen) = 0;
    
    /* Open folder_path in shared read mode and fill folder_handle */

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in create_fcnotify_data", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pvalue != NULL)
        {
            if(pvalue->folder_handle != NULL)
            {
                CloseHandle(pvalue->folder_handle);
                pvalue->folder_handle = NULL;
            }

            alloc_sfree(palloc, pvalue);
            pvalue = NULL;
        }
    }

    dprintverbose("end create_fcnotify_data");

    return result;
}

static void destroy_fcnotify_data(fcnotify_context * pnotify, fcnotify_value * pvalue)
{
    dprintverbose("start destroy_fcnotify_data");

    if(pvalue != NULL)
    {
        _ASSERT(pnotify->processid == pvalue->owner_pid);

        if(pvalue->folder_handle != NULL)
        {
            CloseHandle(pvalue->folder_handle);
            pvalue->folder_handle = NULL;
        }

        alloc_sfree(pnotify->fcalloc, pvalue);
        pvalue = NULL;
    }

    dprintverbose("end destroy_fcnotify_data");
}

static void add_fcnotify_entry(fcnotify_context * pnotify, unsigned int index, fcnotify_value * pvalue)
{
    fcnotify_header * fcheader = NULL;
    fcnotify_value *  pcheck   = NULL;

    dprintverbose("start add_fcnotify_entry");

    _ASSERT(pnotify             != NULL);
    _ASSERT(pvalue              != NULL);
    _ASSERT(pvalue->folder_path != 0);
    
    fcheader = pnotify->fcheader;
    pcheck = FCNOTIFY_VALUE(pnotify->fcalloc, fcheader->values[index]);

    while(pcheck != NULL)
    {
        if(pcheck->next_value == 0)
        {
            break;
        }
        
        pcheck = FCNOTIFY_VALUE(pnotify->fcalloc, pcheck->next_value);
    }

    if(pcheck != NULL)
    {
        pcheck->next_value = alloc_get_valueoffset(pnotify->fcalloc, pvalue);
        pvalue->next_value = 0;
        pvalue->prev_value = alloc_get_valueoffset(pnotify->fcalloc, pcheck);
    }
    else
    {
        fcheader->values[index] = alloc_get_valueoffset(pnotify->fcalloc, pvalue);
        pvalue->next_value = 0;
        pvalue->prev_value = 0;
    }

    fcheader->active_count++;

    dprintverbose("end add_fcnotify_entry");
    return;
}

static void remove_fnotify_entry(fcnotify_context * pnotify, unsigned int index, fcnotify_value * pvalue)
{
    alloc_context *   palloc  = NULL;
    fcnotify_header * header  = NULL;
    fcnotify_value *  ptemp   = NULL;

    dprintverbose("start remove_fcnotify_entry");

    _ASSERT(pcache            != NULL);
    _ASSERT(pvalue            != NULL);
    _ASSERT(pvalue->file_path != 0);

    header = pnotify->fcheader;
    palloc = pnotify->fcalloc;

    header->active_count--;
    if(pvalue->prev_value == 0)
    {
        header->values[index] = pvalue->next_value;
        if(pvalue->next_value != 0)
        {
            ptemp = FCNOTIFY_VALUE(palloc, pvalue->next_value);
            ptemp->prev_value = 0;
        }
    }
    else
    {
        ptemp = FCNOTIFY_VALUE(palloc, pvalue->prev_value);
        ptemp->next_value = pvalue->next_value;

        if(pvalue->next_value != 0)
        {
            ptemp = FCNOTIFY_VALUE(palloc, pvalue->next_value);
            ptemp->prev_value = pvalue->prev_value;
        }
    }

    destroy_fcnotify_data(pnotify, pvalue);
    pvalue = NULL;
    
    dprintverbose("end remove_fcnotify_entry");
    return;
}

int fcnotify_create(fcnotify_context ** ppnotify)
{
    int                result   = NONFATAL;
    fcnotify_context * pcontext = NULL;

    dprintverbose("start fcnotify_create");

    _ASSERT(ppnotify != NULL);
    *ppnotify = NULL;
    
    pcontext = (fcnotify_context *)alloc_pemalloc(sizeof(fcnotify_context));
    if(pcontext == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pcontext->islocal       = 0;
    pcontext->isshutting    = 0;
    pcontext->iswow64       = 0;
    pcontext->processid     = 0;

    pcontext->fcmemaddr     = NULL;
    pcontext->fcheader      = NULL;
    pcontext->fcalloc       = NULL;
    pcontext->fclock        = NULL;

    pcontext->listen_thread = NULL;
    pcontext->port_handle   = NULL;
    pcontext->pidhandles    = NULL;

    *ppnotify = pcontext;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in fcnotify_create", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    dprintverbose("end fcnotify_create");

    return result;
}

void fcnotify_destroy(fcnotify_context * pnotify)
{
    dprintverbose("start fcnotify_destroy");

    if(pnotify != NULL)
    {
        alloc_pefree(pnotify);
        pnotify = NULL;
    }

    dprintverbose("end fcnotify_destroy");

    return;
}

int fcnotify_initialize(fcnotify_context * pnotify, unsigned short islocal, alloc_context * palloc, unsigned int filecount TSRMLS_DC)
{
    int               result   = NONFATAL;
    unsigned short    locktype = LOCK_TYPE_SHARED;
    fcnotify_header * header   = NULL;
    unsigned int      msize    = 0;

    _ASSERT(pnotify != NULL);
    _ASSERT(palloc  != NULL);
    _ASSERT(palloc->memaddr != NULL);

    if(islocal)
    {
        locktype = LOCK_TYPE_LOCAL;
        pnotify->islocal = islocal;
    }

    pnotify->islocal   = islocal;
    pnotify->processid = WCG(fmapgdata)->pid;
    pnotify->fcalloc   = palloc;
    pnotify->fcmemaddr = palloc->memaddr;

    /* Get memory for fcnotify header */
    msize = sizeof(fcnotify_header) + (filecount - 1) * sizeof(size_t);
    pnotify->fcheader = (fcnotify_header *)alloc_get_cacheheader(pnotify->fcalloc, msize, CACHE_TYPE_FCNOTIFY);
    if(pnotify->fcheader == NULL)
    {
        result = FATAL_FCNOTIFY_INITIALIZE;
        goto Finished;
    }

    header = pnotify->fcheader;
 
    /* Create reader writer lock for the file change notification hashtable */
    result = lock_create(&pnotify->fclock);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = lock_initialize(pnotify->fclock, "FILE_CHANGE_NOTIFY", 0, locktype, LOCK_USET_SREAD_XWRITE, &header->rdcount TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Create IO completion port */
    pnotify->port_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
    if(pnotify->port_handle == NULL)
    {
        result = FATAL_FCNOTIFY_INITIALIZE;
        goto Finished;
    }

    /* Create listener thread */
    pnotify->listen_thread = CreateThread(NULL, 0, change_notification_thread, NULL, 0, NULL);
    if(pnotify->listen_thread == NULL)
    {
        result = FATAL_FCNOTIFY_INITIALIZE;
        goto Finished;
    }

    /* Create pidhandles hashtable */
    pnotify->pidhandles = (HashTable *)alloc_pemalloc(sizeof(HashTable));
    if(pnotify->pidhandles == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    zend_hash_init(pnotify->pidhandles, 0, NULL, NULL, 1);

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in fcnotify_initialize", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pnotify->port_handle != NULL)
        {
            CloseHandle(pnotify->port_handle);
            pnotify->port_handle = NULL;
        }

        if(pnotify->listen_thread != NULL)
        {
            CloseHandle(pnotify->listen_thread);
            pnotify->listen_thread = NULL;
        }

        if(pnotify->fclock != NULL)
        {
            lock_terminate(pnotify->fclock);
            lock_destroy(pnotify->fclock);

            pnotify->fclock = NULL;
        }

        if(pnotify->pidhandles != NULL)
        {
            zend_hash_destroy(pnotify->pidhandles);
            alloc_pefree(pnotify->pidhandles);

            pnotify->pidhandles = NULL;
        }
        
        pnotify->fcheader = NULL;
        pnotify->fcalloc  = NULL;
    }

    return result;
}

void fcnotify_terminate(fcnotify_context * pnotify)
{
    if(pnotify != NULL)
    {
        if(pnotify->port_handle != NULL)
        {
            CloseHandle(pnotify->port_handle);
            pnotify->port_handle = NULL;
        }

        if(pnotify->listen_thread != NULL)
        {
            CloseHandle(pnotify->listen_thread);
            pnotify->listen_thread = NULL;
        }

        if(pnotify->fclock != NULL)
        {
            lock_terminate(pnotify->fclock);
            lock_destroy(pnotify->fclock);

            pnotify->fclock = NULL;
        }

        if(pnotify->pidhandles != NULL)
        {
            zend_hash_destroy(pnotify->pidhandles);
            alloc_pefree(pnotify->pidhandles);

            pnotify->pidhandles = NULL;
        }
        
        pnotify->fcheader = NULL;
        pnotify->fcalloc  = NULL;
    }

    return;
}

void fcnotify_runtest()
{
    return;
}
