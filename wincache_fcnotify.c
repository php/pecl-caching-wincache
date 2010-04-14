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

#define PROCESS_IS_ALIVE              0
#define PROCESS_IS_DEAD               1
#define FCNOTIFY_VALUE(p, o)          ((fcnotify_value *)alloc_get_cachevalue(p, o))
#define FCNOTIFY_TERMKEY              ((ULONG_PTR)-1)
#define DWORD_MAX                     0xFFFFFFFF
#define SCAVENGER_FREQUENCY           900000

static unsigned int WINAPI change_notification_thread(void * parg);
static void WINAPI process_change_notification(aplist_context * pcache, unsigned int bytecount, OVERLAPPED * poverlapped);
static int  findfolder_in_cache(fcnotify_context * pnotify, const char * folderpath, unsigned int index, fcnotify_value ** ppvalue);
static void register_directory_changes(HANDLE hfolder, BYTE * pfni, OVERLAPPED * poverlapped);
static int  create_fcnotify_data(fcnotify_context * pnotify, const char * folderpath, fcnotify_value ** ppvalue);
static void destroy_fcnotify_data(fcnotify_context * pnotify, fcnotify_value * pvalue);
static void add_fcnotify_entry(fcnotify_context * pnotify, unsigned int index, fcnotify_value * pvalue);
static void remove_fcnotify_entry(fcnotify_context * pnotify, unsigned int index, fcnotify_value * pvalue);
static int  pidhandles_apply(void * pdestination TSRMLS_DC);
static void run_fcnotify_scavenger(fcnotify_context * pnotify);
static unsigned char process_alive_check(fcnotify_context * pnotify, unsigned int ownerpid);

static unsigned int WINAPI change_notification_thread(void * parg)
{
    fcnotify_context * pnotify   = NULL;
    zend_bool          retvalue  = 0;
    unsigned int       dwerror   = 0;
    unsigned int       bytecount = 0;
    ULONG_PTR          compkey   = 0;
    OVERLAPPED *       poverlap  = NULL;

    pnotify = (fcnotify_context *)parg;

    while(1)
    {
        compkey   = 0;
        bytecount = 0;
        poverlap  = NULL;

        /* Get completion port message. Ok to listen infinitely */
        /* as terminate is going to send termination key */
        retvalue = GetQueuedCompletionStatus(pnotify->port_handle, &bytecount, &compkey, &poverlap, INFINITE);

        /* process change notification if success and key is non termination key */
        dwerror = (retvalue ? ERROR_SUCCESS : GetLastError());
        if(dwerror == ERROR_SUCCESS)
        {
            if(compkey == FCNOTIFY_TERMKEY)
            {
                break;
            }
            else if(poverlap != NULL)
            {
               process_change_notification(pnotify->fcaplist, bytecount, poverlap);
            }
        }
    }

    return ERROR_SUCCESS;
}

static void WINAPI process_change_notification(aplist_context * pcache, unsigned int bytecount, OVERLAPPED * poverlapped)
{
    HRESULT                   hr        = S_OK;
    fcnotify_listen *         plistener = NULL;
    FILE_NOTIFY_INFORMATION * pnitem    = NULL;
    FILE_NOTIFY_INFORMATION * pnnext    = NULL;
    wchar_t *                 pwchar    = NULL;
    char *                    pfname    = NULL;
    unsigned int              fnlength  = 0;

    dprintimportant("start process_change_notification");

    _ASSERT(pcache      != NULL);
    _ASSERT(poverlapped != NULL);

    plistener = CONTAINING_RECORD(poverlapped, fcnotify_listen, overlapped);
    _ASSERT(plistener != NULL);

    if(bytecount == 0)
    {
        /* TBD?? If no bytes were written flush all files under this folder */
    }
    else
    {
        pnnext = (FILE_NOTIFY_INFORMATION *)plistener->fninfo;
        while(pnnext != NULL)
        {
            pnitem = pnnext;
            pnnext = (FILE_NOTIFY_INFORMATION *)((char *)pnitem + pnitem->NextEntryOffset);

            fnlength = pnitem->FileNameLength / 2;

            pwchar = (wchar_t *)alloc_pemalloc((fnlength + 1) * sizeof(wchar_t));
            pfname = (char *)alloc_pemalloc(fnlength + 1);
            if(pwchar == NULL || pfname == NULL)
            {
                hr = E_OUTOFMEMORY;
                goto Finished;
            }

            ZeroMemory(pwchar, (fnlength + 1) * sizeof(wchar_t));
            wcsncpy(pwchar, pnitem->FileName, fnlength);

            ZeroMemory(pfname, fnlength + 1);
            if(!WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, pwchar, fnlength, pfname, fnlength, NULL, NULL))
            {
                hr = GetLastError();
                goto Finished;
            }

            dprintalways("received change notification for file = %s\\%s", plistener->folder_path, pfname);
            aplist_mark_changed(pcache, plistener->folder_path, pfname);

            alloc_pefree(pwchar);
            pwchar = NULL;

            alloc_pefree(pfname);
            pfname = NULL;

            if(pnitem == pnnext)
            {
                break;
            }
        }
    }

Finished:

    /* register for change notification again */
    register_directory_changes(plistener->folder_handle, plistener->fninfo, poverlapped);

    dprintimportant("end process_change_notification");

    return;
}

static int findfolder_in_cache(fcnotify_context * pnotify, const char * folderpath, unsigned int index, fcnotify_value ** ppvalue)
{
    int               result  = NONFATAL;
    fcnotify_header * pheader = NULL;
    fcnotify_value *  pvalue  = NULL;

    dprintimportant("start findfolder_in_cache");

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

        pvalue = FCNOTIFY_VALUE(pnotify->fcalloc, pvalue->next_value);
    }

    dprintimportant("end findfolder_in_cache");

    return result;
}

static void register_directory_changes(HANDLE hfolder, BYTE * pfni, OVERLAPPED * poverlapped)
{
    unsigned int cflags    = 0;
    unsigned int bytecount = 0;
    
    _ASSERT(hfolder     != NULL);
    _ASSERT(hfolder     != INVALID_HANDLE_VALUE);
    _ASSERT(pfni        != NULL);
    _ASSERT(poverlapped != NULL);

    /* file name, last write, attributes and security change should be delivered */
    cflags = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY;
    ReadDirectoryChangesW(hfolder, pfni, 1024, FALSE, cflags, &bytecount, poverlapped, NULL);

    return;
}

static int create_fcnotify_data(fcnotify_context * pnotify, const char * folderpath, fcnotify_value ** ppvalue)
{
    int               result    = NONFATAL;
    fcnotify_value *  pvalue    = NULL;
    alloc_context *   palloc    = NULL;
    unsigned int      pathlen   = 0;
    char *            paddr     = NULL;
    HANDLE            phandle   = NULL;
    unsigned int      fshare    = 0;
    unsigned int      flags     = 0;
    fcnotify_listen * plistener = NULL;

    dprintimportant("start create_fcnotify_data");

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
    pvalue->plistener     = NULL;
    pvalue->refcount      = 0;
    pvalue->prev_value    = 0;
    pvalue->next_value    = 0;

    paddr = (char *)(pvalue + 1);
    memcpy_s(paddr, pathlen, folderpath, pathlen);
    *(paddr + pathlen) = 0;
    pvalue->folder_path = ((char *)paddr) - pnotify->fcmemaddr;

    /* Allocate memory for listener locally */
    plistener = (fcnotify_listen *)alloc_pemalloc(sizeof(fcnotify_listen));
    if(plistener == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    ZeroMemory(plistener, sizeof(fcnotify_listen));

    /* Open folder_path and attach it to completion port */
    fshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    flags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED;

    phandle = CreateFile(folderpath, FILE_LIST_DIRECTORY, fshare, NULL, OPEN_EXISTING, flags, NULL);
    if(phandle == INVALID_HANDLE_VALUE)
    {
        result = FATAL_FCNOTIFY_CREATEFILE;
        goto Finished;
    }

    if(CreateIoCompletionPort(phandle, pnotify->port_handle, (ULONG_PTR)0, 0) == NULL)
    {
        result = FATAL_FCNOTIFY_COMPPORT;
        goto Finished;
    }

    register_directory_changes(phandle, plistener->fninfo, &plistener->overlapped);

    plistener->folder_path   = alloc_pestrdup(folderpath);
    plistener->folder_handle = phandle;
    phandle = NULL;

    pvalue->plistener = plistener;
    plistener = NULL;

    *ppvalue = pvalue;

Finished:

    if(phandle != NULL)
    {
        CloseHandle(phandle);
        phandle = NULL;
    }

    if(plistener != NULL)
    {
        alloc_pefree(plistener);
        plistener = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in create_fcnotify_data", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pvalue != NULL)
        {
            if(pvalue->plistener != NULL)
            {
                if(pvalue->plistener->folder_path != NULL)
                {
                    alloc_pefree(pvalue->plistener->folder_path);
                    pvalue->plistener->folder_path = NULL;
                }

                if(pvalue->plistener->folder_handle != NULL)
                {
                    CloseHandle(pvalue->plistener->folder_handle);
                    pvalue->plistener->folder_handle = NULL;
                }

                alloc_pefree(pvalue->plistener);
                pvalue->plistener = NULL;
            }

            alloc_sfree(palloc, pvalue);
            pvalue = NULL;
        }
    }

    dprintimportant("end create_fcnotify_data");

    return result;
}

static void destroy_fcnotify_data(fcnotify_context * pnotify, fcnotify_value * pvalue)
{
    dprintimportant("start destroy_fcnotify_data");

    if(pvalue != NULL)
    {
        /* Free memory occupied by plistener and close the handle */
        /* if owner process is destroying fcnotify data */
        if(pnotify->processid == pvalue->owner_pid && pvalue->plistener != NULL)
        {
            if(pvalue->plistener->folder_path != NULL)
            {
                alloc_pefree(pvalue->plistener->folder_path);
                pvalue->plistener->folder_path = NULL;
            }

            if(pvalue->plistener->folder_handle != NULL)
            {
                CloseHandle(pvalue->plistener->folder_handle);
                pvalue->plistener->folder_handle = NULL;
            }

            alloc_pefree(pvalue->plistener);
            pvalue->plistener = NULL;
        }

        alloc_sfree(pnotify->fcalloc, pvalue);
        pvalue = NULL;
    }

    dprintimportant("end destroy_fcnotify_data");
}

static void add_fcnotify_entry(fcnotify_context * pnotify, unsigned int index, fcnotify_value * pvalue)
{
    fcnotify_header * fcheader = NULL;
    fcnotify_value *  pcheck   = NULL;

    dprintimportant("start add_fcnotify_entry");

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

    fcheader->itemcount++;

    dprintimportant("end add_fcnotify_entry");
    return;
}

static void remove_fcnotify_entry(fcnotify_context * pnotify, unsigned int index, fcnotify_value * pvalue)
{
    alloc_context *   palloc  = NULL;
    fcnotify_header * header  = NULL;
    fcnotify_value *  ptemp   = NULL;

    dprintimportant("start remove_fcnotify_entry");

    _ASSERT(pnotify             != NULL);
    _ASSERT(pvalue              != NULL);
    _ASSERT(pvalue->folder_path != 0);

    header = pnotify->fcheader;
    palloc = pnotify->fcalloc;

    header->itemcount--;
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
    
    dprintimportant("end remove_fcnotify_entry");
    return;
}

/* Public functions */
int fcnotify_create(fcnotify_context ** ppnotify)
{
    int                result   = NONFATAL;
    fcnotify_context * pcontext = NULL;

    dprintimportant("start fcnotify_create");

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
    pcontext->lscavenge     = 0;

    pcontext->fcmemaddr     = NULL;
    pcontext->fcheader      = NULL;
    pcontext->fcalloc       = NULL;
    pcontext->fclock        = NULL;
    pcontext->fcaplist      = NULL;

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

    dprintimportant("end fcnotify_create");

    return result;
}

void fcnotify_destroy(fcnotify_context * pnotify)
{
    dprintimportant("start fcnotify_destroy");

    if(pnotify != NULL)
    {
        alloc_pefree(pnotify);
        pnotify = NULL;
    }

    dprintimportant("end fcnotify_destroy");

    return;
}

int fcnotify_initialize(fcnotify_context * pnotify, unsigned short islocal, void * paplist, alloc_context * palloc, unsigned int filecount TSRMLS_DC)
{
    int               result   = NONFATAL;
    unsigned short    locktype = LOCK_TYPE_SHARED;
    fcnotify_header * header   = NULL;
    unsigned int      msize    = 0;

    _ASSERT(pnotify != NULL);
    _ASSERT(paplist != NULL);
    _ASSERT(palloc  != NULL);
    _ASSERT(palloc->memaddr != NULL);

    if(islocal)
    {
        locktype = LOCK_TYPE_LOCAL;
        pnotify->islocal = islocal;
    }

    pnotify->islocal   = islocal;
    pnotify->fcaplist  = paplist;
    pnotify->fcalloc   = palloc;

    pnotify->processid = WCG(fmapgdata)->pid;
    pnotify->fcmemaddr = palloc->memaddr;
    pnotify->lscavenge = GetTickCount();

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

    result = lock_initialize(pnotify->fclock, "FILE_CHANGE_NOTIFY", 1, locktype, LOCK_USET_SREAD_XWRITE, &header->rdcount TSRMLS_CC);
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
    pnotify->listen_thread = CreateThread(NULL, 0, change_notification_thread, (void *)pnotify, 0, NULL);
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

        if(pnotify->listen_thread != NULL)
        {
            CloseHandle(pnotify->listen_thread);
            pnotify->listen_thread = NULL;
        }

        if(pnotify->port_handle != NULL)
        {
            CloseHandle(pnotify->port_handle);
            pnotify->port_handle = NULL;
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
        
        pnotify->fcaplist = NULL;
        pnotify->fcheader = NULL;
        pnotify->fcalloc  = NULL;
    }

    return result;
}

void fcnotify_initheader(fcnotify_context * pnotify, unsigned int filecount)
{
     fcnotify_header * pheader = NULL;

     dprintimportant("start fcnotify_initheader");

     _ASSERT(pnotify           != NULL);
     _ASSERT(pnotify->fcheader != NULL);
     _ASSERT(filecount         >  0);

     pheader = pnotify->fcheader;
     _ASSERT(pheader->values != NULL);

     /* This method is called by aplist_initialize which is */
     /* taking care of blocking other processes */
     /* Also rdcount can be safely set to 0 as lock is not active */
     pheader->rdcount       = 0;
     pheader->itemcount     = 0;
     pheader->valuecount    = filecount;
     memset((void *)pheader->values, 0, sizeof(size_t) * filecount);

     dprintimportant("end fcnotify_initheader");

     return;
}

void fcnotify_terminate(fcnotify_context * pnotify)
{
    dprintimportant("start fcnotify_terminate");

    if(pnotify != NULL)
    {
        if(pnotify->listen_thread != NULL)
        {
            if(pnotify->port_handle != NULL)
            {
                /* Post termination key to completion port to terminate thread */
                PostQueuedCompletionStatus(pnotify->port_handle, 0, FCNOTIFY_TERMKEY, NULL);

                /* Wait for thread to finish */
                WaitForSingleObject(pnotify->listen_thread, INFINITE);
            }

            CloseHandle(pnotify->listen_thread);
            pnotify->listen_thread = NULL;
        }

        if(pnotify->port_handle != NULL)
        {
            CloseHandle(pnotify->port_handle);
            pnotify->port_handle = NULL;
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

        pnotify->fcaplist = NULL;
        pnotify->fcheader = NULL;
        pnotify->fcalloc  = NULL;
    }

    dprintimportant("end fcnotify_terminate");

    return;
}

static int pidhandles_apply(void * pdestination TSRMLS_DC)
{
    HANDLE *      hprocess = NULL;
    unsigned int  exitcode = 0;

    _ASSERT(pdestination != NULL);
    hprocess = (HANDLE *)pdestination;

    if(GetExitCodeProcess(*hprocess, &exitcode) && exitcode != STILL_ACTIVE)
    {
        CloseHandle(*hprocess);
        return ZEND_HASH_APPLY_REMOVE;
    }
    else
    {
        return ZEND_HASH_APPLY_KEEP;
    }
}

static void run_fcnotify_scavenger(fcnotify_context * pnotify)
{
    fcnotify_header * pheader    = NULL;
    fcnotify_value *  pvalue     = NULL;
    fcnotify_value *  pnext      = NULL;
    unsigned int      index      = 0;
    unsigned int      count      = 0;
    HashTable *       phashtable = NULL;

    TSRMLS_FETCH();
    dprintimportant("start run_fcnotify_scavenger");

    pheader = pnotify->fcheader;
    count = pheader->valuecount;

    phashtable = pnotify->pidhandles;

    /* Go through all the entries and remove entries for which refcount is 0 */\
    /* Do it only for processes which are dead or if this was the owner pid */
    lock_writelock(pnotify->fclock);

    for(index = 0; index < count; index++)
    {
        pnext = FCNOTIFY_VALUE(pnotify->fcalloc, pheader->values[index]);
        while(pnext != NULL)
        {
            pvalue = pnext;
            pnext = FCNOTIFY_VALUE(pnotify->fcalloc, pvalue->next_value);

            /* process alive check will remove entry from pidhandles if necessary */
            if(pvalue->refcount == 0 &&
               (pvalue->owner_pid == pnotify->processid || process_alive_check(pnotify, pvalue->owner_pid) == PROCESS_IS_DEAD))
            {
                remove_fcnotify_entry(pnotify, index, pvalue);
                pvalue = NULL;
            }
        }
    }

    lock_writeunlock(pnotify->fclock);

    /* Go through pidhandles table and remove entries for dead processes */
    zend_hash_apply(phashtable, pidhandles_apply TSRMLS_CC);
    pnotify->lscavenge = GetTickCount();

    dprintimportant("end run_fcnotify_scavenger");
}

static unsigned char process_alive_check(fcnotify_context * pnotify, unsigned int ownerpid)
{
    HashTable *   phashtable = NULL;
    HANDLE        hprocess   = NULL;
    HANDLE *      phdata     = NULL;
    unsigned char listenp    = PROCESS_IS_ALIVE;
    unsigned int  exitcode   = 0;
    HANDLE        htoken     = NULL;
    unsigned int  bthread    = 0;

    _ASSERT(pnotify  != NULL);
    _ASSERT(ownerpid >  0);

    /* If the check is for this process, return alive */
    if(pnotify->processid == ownerpid)
    {
        goto Finished;
    }

    phashtable = pnotify->pidhandles;

    if(zend_hash_index_find(phashtable, (ulong)ownerpid, (void **)&phdata) == FAILURE)
    {
        /* Check if impersonation is enabled */
        /* If it is, get impersonated token and set it back after calling OpenProcess */
        bthread = OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE, TRUE, &htoken);
        if(bthread)
        {
            RevertToSelf();
        }

        hprocess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, ownerpid);

        if(bthread)
        {
            SetThreadToken(NULL, htoken);
            CloseHandle(htoken);
        }

        if(hprocess != NULL)
        {
            /* Process is present. Keep the handle around to save OpenProcess calls */
            zend_hash_index_update(phashtable, (ulong)ownerpid, (void **)&hprocess, sizeof(HANDLE), NULL);
        }
        else
        {
            /* OpenProcess failure means process is gone */
            listenp = PROCESS_IS_DEAD;
        }
    }
    else
    {
        hprocess = *phdata;
        if(GetExitCodeProcess(hprocess, &exitcode) && exitcode != STILL_ACTIVE)
        {
            /* GetProcessId failure means process is gone */
            CloseHandle(hprocess);
            zend_hash_index_del(phashtable, (ulong)ownerpid);

            listenp = PROCESS_IS_DEAD;
        }
    }

Finished:

    return listenp;
}

int fcnotify_check(fcnotify_context * pnotify, const char * filepath, size_t offset, size_t * poffset)
{
    int               result     = NONFATAL;
    unsigned int      flength    = 0;
    char *            folderpath = NULL;
    unsigned char     allocated  = 0;
    unsigned int      index      = 0;
    fcnotify_header * pheader    = NULL;
    fcnotify_value *  pvalue     = NULL;
    fcnotify_value *  ptemp      = NULL;
    fcnotify_value *  pnewval    = NULL;
    unsigned char     listenp    = 0;
    unsigned char     flock      = 0;
    unsigned int      cticks     = 0;

    dprintimportant("start fcnotify_check");

    _ASSERT(pnotify  != NULL);
    _ASSERT(filepath != NULL || offset != 0);
    _ASSERT(poffset  != NULL);

    *poffset = 0;
    pheader = pnotify->fcheader;

    /* Run scavenger every SCAVENGER_FREQUENCY milliseconds */
    cticks = GetTickCount();
    if((cticks < pnotify->lscavenge && ((DWORD_MAX - pnotify->lscavenge) + cticks) >= SCAVENGER_FREQUENCY) ||
       (cticks > pnotify->lscavenge && cticks - pnotify->lscavenge >= SCAVENGER_FREQUENCY))
    {
        run_fcnotify_scavenger(pnotify);
    }

    if(offset != 0)
    {
        /* Use offset to get to fcnotify_value */
        pvalue = FCNOTIFY_VALUE(pnotify->fcalloc, offset);
        folderpath = (char *)(pnotify->fcmemaddr + pvalue->folder_path);
        index = utils_getindex(folderpath, pheader->valuecount);
    }
    else
    {
        /* Get folderpath from filepath */
        flength = strlen(filepath);

        folderpath = alloc_emalloc(flength);
        if(folderpath == NULL)
        {
            result = FATAL_OUT_OF_LMEMORY;
            goto Finished;
        }

        allocated = 1;

        result = utils_filefolder(filepath, flength, folderpath, flength);
        if(FAILED(result))
        {
            goto Finished;
        }

        index = utils_getindex(folderpath, pheader->valuecount);

        /* Look if folder path  is already present in cache */
        lock_readlock(pnotify->fclock);
        result = findfolder_in_cache(pnotify, folderpath, index, &pvalue);
        lock_readunlock(pnotify->fclock);

        if(FAILED(result))
        {
            goto Finished;
        }
    }

    if(pvalue != NULL)
    {
        /* Check if owner_pid process is still there */
        listenp = process_alive_check(pnotify, pvalue->owner_pid);
    }
    else
    {
        /* If folder is not even there yet, add process listener */
        listenp = 1;
    }

    /* If folder is not there, register for change notification */
    /* and add the folder to the hashtable */
    if(listenp)
    {
        lock_writelock(pnotify->fclock);
        flock = 1;

        /* Check again to make sure no one else already registered */
        result = findfolder_in_cache(pnotify, folderpath, index, &ptemp);
        if(FAILED(result))
        {
            goto Finished;
        }

        if(ptemp != NULL)
        {
            if(pvalue != NULL && pvalue->owner_pid == ptemp->owner_pid)
            {
                /* Delete existing entry whose owner is now gone */
                /* Ok to delete fcnotify entry by a non-owner pid if owner is gone */
                remove_fcnotify_entry(pnotify, index, pvalue);
                pvalue = NULL;
            }
            else
            {
                /* Some other process added the entry before this process could */
                pvalue = ptemp;
                ptemp = NULL;
            }
        }
        
        if(pvalue == NULL)
        {
            /* Doing create_fcnotify_data inside write lock to keep things simpler */
            result = create_fcnotify_data(pnotify, folderpath, &ptemp);
            if(FAILED(result))
            {
                goto Finished;
            }

            pvalue = ptemp;
            ptemp  = NULL;
            add_fcnotify_entry(pnotify, index, pvalue);
        }

        lock_writeunlock(pnotify->fclock);
        flock = 0;
    }
    
    _ASSERT(pvalue != NULL);

    /* Increment refcount if check is called for the first time */
    if(filepath != NULL)
    {
        InterlockedIncrement(&pvalue->refcount);
    }

    *poffset = ((char *)pvalue - pnotify->fcmemaddr);

Finished:

    if(flock)
    {
        lock_writeunlock(pnotify->fclock);
        flock = 0;
    }

    if(allocated && folderpath != NULL)
    {
        alloc_efree(folderpath);
        folderpath = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in fcnotify_check", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(ptemp != NULL)
        {
            destroy_fcnotify_data(pnotify, ptemp);
            ptemp = NULL;
        }
    }

    dprintimportant("end fcnotify_check");

    return result;
}

void fcnotify_close(fcnotify_context * pnotify, size_t offset)
{
    fcnotify_value * pvalue = NULL;
    unsigned int     index  = 0;

    dprintimportant("start fcnotify_close");

    _ASSERT(pnotify != NULL);
    _ASSERT(offset  >  0);

    /* Just decrement the refcount. Scavenger will take care of deleting the entry */
    pvalue = FCNOTIFY_VALUE(pnotify->fcalloc, offset);
    InterlockedDecrement(&pvalue->refcount);

    dprintimportant("end fcnotify_close");

    return;
}

int fcnotify_getinfo(fcnotify_context * pnotify, zend_bool summaryonly, fcnotify_info ** ppinfo)
{
    int                    result  = NONFATAL;
    fcnotify_info *        pcinfo  = NULL;
    fcnotify_entry_info *  peinfo  = NULL;
    fcnotify_entry_info *  ptemp   = NULL;
    fcnotify_value *       pvalue  = NULL;
    unsigned char          flock   = 0;
    size_t                 offset  = 0;
    unsigned int           count   = 0;
    unsigned int           index   = 0;

    dprintverbose("start fcnotify_getinfo");

    _ASSERT(pnotify != NULL);
    _ASSERT(ppinfo  != NULL);

    *ppinfo = NULL;

    pcinfo = (fcnotify_info *)alloc_emalloc(sizeof(fcnotify_info));
    if(pcinfo == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    lock_readlock(pnotify->fclock);
    flock = 1;

    pcinfo->itemcount = pnotify->fcheader->itemcount;
    pcinfo->entries   = NULL;

    /* Leave count to 0 if only summary is needed */
    if(!summaryonly)
    {
        count = pnotify->fcheader->valuecount;
    }

    for(index = 0; index < count; index++)
    {
        offset = pnotify->fcheader->values[index];
        if(offset == 0)
        {
            continue;
        }

        pvalue = (fcnotify_value *)alloc_get_cachevalue(pnotify->fcalloc, offset);
        while(pvalue != NULL)
        {
            ptemp = (fcnotify_entry_info *)alloc_emalloc(sizeof(fcnotify_entry_info));
            if(ptemp == NULL)
            {
                result = FATAL_OUT_OF_LMEMORY;
                goto Finished;
            }

            _ASSERT(pvalue->folder_path != 0);

            ptemp->folderpath = alloc_estrdup(pnotify->fcmemaddr + pvalue->folder_path);
            ptemp->ownerpid   = pvalue->owner_pid;
            ptemp->filecount  = pvalue->refcount;
            ptemp->next       = NULL;

            if(peinfo != NULL)
            {
                peinfo->next = ptemp;
            }
            else
            {
                pcinfo->entries = ptemp;
            }

            peinfo = ptemp;

            offset = pvalue->next_value;
            if(offset == 0)
            {
                break;
            }

            pvalue = (fcnotify_value *)alloc_get_cachevalue(pnotify->fcalloc, offset);
        }
    }

    *ppinfo = pcinfo;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(flock)
    {
        lock_readunlock(pnotify->fclock);
        flock = 0;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in fcnotify_getinfo", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pcinfo != NULL)
        {
            peinfo = pcinfo->entries;
            while(peinfo != NULL)
            {
                ptemp = peinfo;
                peinfo = peinfo->next;

                if(ptemp->folderpath != NULL)
                {
                    alloc_efree(ptemp->folderpath);
                    ptemp->folderpath = NULL;
                }

                alloc_efree(ptemp);
                ptemp = NULL;
            }

            alloc_efree(pcinfo);
            pcinfo = NULL;
        }
    }

    dprintverbose("start fcnotify_getinfo");

    return result;
}

void fcnotify_freeinfo(fcnotify_info * pinfo)
{
    fcnotify_entry_info * peinfo = NULL;
    fcnotify_entry_info * petemp = NULL;

    if(pinfo != NULL)
    {
        peinfo = pinfo->entries;
        while(peinfo != NULL)
        {
            petemp = peinfo;
            peinfo = peinfo->next;

            if(petemp->folderpath != NULL)
            {
                alloc_efree(petemp->folderpath);
                petemp->folderpath = NULL;
            }

            alloc_efree(petemp);
            petemp = NULL;
        }

        alloc_efree(pinfo);
        pinfo = NULL;
    }
}

void fcnotify_runtest()
{
    return;
}