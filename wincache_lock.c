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
   | Module: wincache_lock.c                                                                      |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

/* Globals */
unsigned short glockid = 1;

/* Create a new lock context */
/* Call lock_initialize to actually create the lock */
int lock_create(lock_context ** pplock)
{
    int            result = NONFATAL;
    lock_context * plock  = NULL;

    dprintverbose("start lock_create");

    _ASSERT( pplock != NULL );
    *pplock = NULL;

    plock = (lock_context *)alloc_pemalloc(sizeof(lock_context));
    if(plock == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    /* Initialize lock_context structure with default values */
    plock->id         = glockid++;
    plock->type       = LOCK_TYPE_INVALID;
    plock->usetype    = LOCK_USET_INVALID;
    plock->state      = LOCK_STATE_INVALID;
    plock->nameprefix = NULL;
    plock->namelen    = 0;
    plock->haccess    = NULL;
    plock->hcanread   = NULL;
    plock->hcanwrite  = NULL;
    plock->hxwrite    = NULL;
    plock->prcount    = NULL;

    /* id is 8 bit field. Keep maximum to 255 */
    if(glockid == 256)
    {
        glockid = 1;
    }

    *pplock = plock;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in lock_create", result);
    }

    dprintverbose("end lock_create");

    return result;
}

/* Destroy the lock context */
void lock_destroy(lock_context * plock)
{
    dprintverbose("start lock_destroy");

    if( plock != NULL )
    {
        _ASSERT(plock->nameprefix == NULL);
        _ASSERT(plock->haccess    == NULL);
        _ASSERT(plock->hcanread   == NULL);
        _ASSERT(plock->hcanwrite  == NULL);
        _ASSERT(plock->hxwrite    == NULL);

        alloc_pefree(plock);
        plock = NULL;
    }

    dprintverbose("end lock_destroy");
}

/* Initialize the lock context with valid information */
/* lock is not ready to use unless initialize is called */
int lock_initialize(lock_context * plock, char * name, unsigned short cachekey, unsigned short type, unsigned short usetype, unsigned int * prcount TSRMLS_DC)
{
    int    result  = NONFATAL;
    char * objname = 0;
    int    namelen = 0;
    int    pid     = 0;
    int    ppid    = 0;

    dprintverbose("start lock_initialize");

    _ASSERT(plock    != NULL);
    _ASSERT(name     != NULL);
    _ASSERT(cachekey != 0);
    _ASSERT(cachekey <= LOCK_NUMBER_MAXIMUM);
    _ASSERT(type     <= LOCK_TYPE_MAXIMUM);
    _ASSERT(usetype  <= LOCK_USET_MAXIMUM);

    if(cachekey > LOCK_NUMBER_MAXIMUM)
    {
        result = FATAL_LOCK_NUMBER_LARGE;
        goto Finished;
    }

    /* If shared reader/writer locks are disabled, make everything an exclusive lock */
    if (WCG(srwlocks) == 0)
    {
        usetype = LOCK_USET_XREAD_XWRITE;
    }

    /* Get the initial length of name prefix */
    namelen = strlen(name);
    _ASSERT(namelen > 0);

    /* If a namesalt is specified, put _<salt> in the name */
    if(WCG(namesalt) != NULL)
    {
        namelen += strlen(WCG(namesalt)) + 1;
    }

    /* Depending on what type of lock to create, get pid and ppid */
    switch(type)
    {
        case LOCK_TYPE_SHARED:
            plock->type  = LOCK_TYPE_SHARED;
            ppid = filemap_getppid(TSRMLS_C);
            namelen += PID_MAX_LENGTH;
            break;
        case LOCK_TYPE_LOCAL:
            plock->type = LOCK_TYPE_LOCAL;
            pid = filemap_getpid(TSRMLS_C);
            namelen += PID_MAX_LENGTH;
            break;
        case LOCK_TYPE_GLOBAL:
            plock->type = LOCK_TYPE_GLOBAL;
            break;
        default:
            _ASSERT(FALSE);
            break;
    }

    /* Add 4 for underscores, 1 for null termination, */
    /* LOCK_NUMBER_MAX_STRLEN for adding cachekey, 2 for 0 (for global) */
    namelen = namelen + LOCK_NUMBER_MAX_STRLEN + 7;

    /* Most synchronization object names are limited to MAX_PATH characters */
    if(namelen >= MAX_PATH - 1)
    {
        result = FATAL_LOCK_LONGNAME;
        goto Finished;
    }

    /* Allocate memory for name prefix */
    plock->nameprefix = (char *)alloc_pemalloc(namelen);
    if(plock->nameprefix == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    objname = (char *)alloc_pemalloc(namelen + 1);
    if(objname == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    ZeroMemory(plock->nameprefix, namelen);
    ZeroMemory(objname, namelen + 1);

    /* Create nameprefix as name_pid_ppid_ */
    if(WCG(namesalt) == NULL)
    {
        _snprintf_s(plock->nameprefix, namelen, namelen - 1, "%s_%u_%u_%u_", name, cachekey, pid, ppid);
    }
    else
    {
        _snprintf_s(plock->nameprefix, namelen, namelen - 1, "%s_%u_%s_%u_%u_", name, cachekey, WCG(namesalt), pid, ppid);
    }

    plock->namelen = strlen(plock->nameprefix);

    strcpy_s(objname, namelen - 1, plock->nameprefix);
    namelen = plock->namelen;

    /* Depending on what type of lock this needs */
    /* to be, create one or two handles */
    switch(usetype)
    {
        case LOCK_USET_SREAD_XWRITE:

            plock->usetype = LOCK_USET_SREAD_XWRITE;

            /* We must get where to keep the reader count */
            _ASSERT(prcount != NULL);
            plock->prcount = prcount;

            /* Create named objects for events and mutexes */
            objname[namelen] = 'A';
            plock->haccess = CreateMutex(NULL, FALSE, objname);
            if(plock->haccess == NULL)
            {
                dprintimportant("Failed to create lock %s due to error %u", objname, error_setlasterror());
                result = FATAL_LOCK_INIT_CREATEMUTEX;
                goto Finished;
            }

            /* If this is the first process which is creating */
            /* the lock, set the reader count to 0 */
            if(GetLastError() != ERROR_ALREADY_EXISTS)
            {
                *plock->prcount = 0;
            }

            objname[namelen] = 'R';
            plock->hcanread = CreateEvent(NULL, TRUE, TRUE, objname);
            if(plock->hcanread == NULL)
            {
                dprintimportant("Failed to create lock %s due to error %u", objname, error_setlasterror());
                result = FATAL_LOCK_INIT_CREATEEVENT;

                goto Finished;
            }

            objname[namelen] = 'W';
            plock->hcanwrite = CreateEvent(NULL, TRUE, TRUE, objname);
            if(plock->hcanwrite == NULL)
            {
                dprintimportant("Failed to create lock %s due to error %u", objname, error_setlasterror());
                result = FATAL_LOCK_INIT_CREATEEVENT;

                goto Finished;
            }

            objname[namelen] = 'X';
            plock->hxwrite = CreateMutex(NULL, FALSE, objname);
            if(plock->hxwrite == NULL)
            {
                dprintimportant("Failed to create lock %s due to error %u", objname, error_setlasterror());
                result = FATAL_LOCK_INIT_CREATEMUTEX;
                goto Finished;
            }

            break;
        case LOCK_USET_XREAD_XWRITE:

            plock->usetype = LOCK_USET_XREAD_XWRITE;

            /* Only create one mutex which will be used */
            /* to synchronize access to read and write */
            objname[namelen] = 'X';
            plock->hxwrite = CreateMutex(NULL, FALSE, objname);
            if( plock->hxwrite == NULL )
            {
                dprintimportant("Failed to create lock %s due to error %u", objname, error_setlasterror());
                result = FATAL_LOCK_INIT_CREATEMUTEX;
                goto Finished;
            }

            break;
        default:
            _ASSERT(FALSE);
            break;
    }

    plock->state = LOCK_STATE_UNLOCKED;
    _ASSERT(SUCCEEDED(result));

Finished:

    if(objname != NULL)
    {
        alloc_pefree(objname);
        objname = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in lock_initialize", result);

        if(plock->haccess != NULL)
        {
            CloseHandle(plock->haccess);
            plock->haccess = NULL;
        }

        if(plock->hcanread != NULL)
        {
            CloseHandle(plock->hcanread);
            plock->hcanread = NULL;
        }

        if(plock->hcanwrite != NULL)
        {
            CloseHandle(plock->hcanwrite);
            plock->hcanwrite = NULL;
        }

        if(plock->hxwrite != NULL)
        {
            CloseHandle(plock->hxwrite);
            plock->hxwrite = NULL;
        }

        if(plock->nameprefix != NULL)
        {
            alloc_pefree(plock->nameprefix);
            plock->nameprefix = NULL;

            plock->namelen = 0;
        }

        plock->type    = LOCK_TYPE_INVALID;
        plock->usetype = LOCK_USET_INVALID;
        plock->state   = LOCK_STATE_INVALID;
        plock->prcount = NULL;
    }

    dprintverbose("end lock_initialize");

    return result;
}

void lock_terminate(lock_context * plock)
{
    dprintverbose("start lock_terminate");

    if(plock != NULL)
    {
        if(plock->haccess != NULL)
        {
            CloseHandle(plock->haccess);
            plock->haccess = NULL;
        }

        if(plock->hcanread != NULL)
        {
            CloseHandle(plock->hcanread);
            plock->hcanread = NULL;
        }

        if(plock->hcanwrite != NULL)
        {
            CloseHandle(plock->hcanwrite);
            plock->hcanwrite = NULL;
        }

        if(plock->hxwrite != NULL)
        {
            CloseHandle(plock->hxwrite);
            plock->hxwrite = NULL;
        }

        if(plock->nameprefix != NULL)
        {
            alloc_pefree(plock->nameprefix);
            plock->nameprefix = NULL;

            plock->namelen = 0;
        }

        plock->type    = LOCK_TYPE_INVALID;
        plock->usetype = LOCK_USET_INVALID;
        plock->state   = LOCK_STATE_INVALID;
        plock->prcount = NULL;
    }

    dprintverbose("end lock_terminate");
}

/* Acquire read lock */
void lock_readlock(lock_context * plock)
{
    long   rcount    = 0;
    HANDLE hArray[2];
    DWORD  ret       = 0;

    dprintverbose("start lock_readlock");

    _ASSERT(plock          != NULL);
    _ASSERT(plock->hxwrite != NULL);

    switch(plock->usetype)
    {
        case LOCK_USET_SREAD_XWRITE:
            /* First try getting a lock on whandle to make sure  */
            /* there is no writer waiting for readers to go to 0 */
            /* Increment reader count and Release writer lock */
            _ASSERT(plock->prcount != NULL);

            hArray[0] = plock->haccess;
            hArray[1] = plock->hcanread;
            ret = WaitForMultipleObjects(2, hArray, TRUE, INFINITE);

            if (ret == WAIT_ABANDONED_0)
            {
                error_setlasterror();
                dprintcritical("lock_readlock: acquired abandoned mutex %sA. Something bad happend in another process!", 
                                plock->nameprefix);
                php_error(E_ERROR, "WINCACHE: lock_readlock: acquired abandoned mutex %sA. Something bad happend in another process!",
                          plock->nameprefix);
            }

            if (ret == WAIT_FAILED)
            {
                dprintcritical("lock_readlock: Failure waiting on shared readlock %s* (%d). Something bad happened!", 
                                plock->nameprefix, error_setlasterror());
                php_error(E_ERROR, "WINCACHE: lock_readlock: Failure waiting on shared readlock %s* (%d). Something bad happened!",
                          plock->nameprefix);

            }

            (*plock->prcount)++;

            ResetEvent(plock->hcanwrite);
            ReleaseMutex(plock->haccess);

            break;

        case LOCK_USET_XREAD_XWRITE:
            /* Simple case. Just lock hxwrite */
            ret = WaitForSingleObject(plock->hxwrite, INFINITE);

            if (ret == WAIT_ABANDONED)
            {
                error_setlasterror();
                dprintcritical("lock_readlock: acquired abandoned mutex %sX. Something bad happend in another process!",
                                plock->nameprefix);
                php_error(E_ERROR, "WINCACHE: lock_readlock: acquired abandoned mutex %sX. Something bad happend in another process!",
                          plock->nameprefix);
            }

            if (ret == WAIT_FAILED)
            {
                dprintcritical("lock_readlock: Failure waiting on readlock %sX (%d). Something bad happened!", 
                                plock->nameprefix, error_setlasterror());
                php_error(E_ERROR, "WINCACHE: lock_readlock: Failure waiting on readlock %sX (%d). Something bad happened!", 
                          plock->nameprefix, error_setlasterror());
            }

            break;

        default:
            _ASSERT(FALSE);
            break;
    }

    plock->state = LOCK_STATE_READLOCK;

    dprintverbose("end lock_readlock");
    return;
}

/* Release read lock */
void lock_readunlock(lock_context * plock)
{
    long rcount = 0;
    DWORD ret   = 0;

    dprintverbose("start lock_readunlock");

    _ASSERT(plock          != NULL);
    _ASSERT(plock->hxwrite != NULL);

    switch(plock->usetype)
    {
        case LOCK_USET_SREAD_XWRITE:
            /* Decrement reader count. If hits zero, set the writer event */
            /* so the writer can proceed */
            _ASSERT(plock->prcount != NULL);
            ret = WaitForSingleObject(plock->haccess, INFINITE);

            if (ret == WAIT_ABANDONED)
            {
                error_setlasterror();
                dprintcritical("lock_readunlock: acquired abandoned mutex %sA. Something bad happend in another process!", 
                                plock->nameprefix);
                php_error(E_ERROR, "WINCACHE: lock_readunlock: acquired abandoned mutex %sA. Something bad happend in another process!",
                          plock->nameprefix);
            }

            if (ret == WAIT_FAILED)
            {
                dprintcritical("lock_readunlock: Failure waiting on shared lock %s* (%d). Something bad happened!", 
                                plock->nameprefix, error_setlasterror());
                php_error(E_ERROR, "WINCACHE: lock_readunlock: Failure waiting on readlock %s* (%d). Something bad happened!", 
                          plock->nameprefix, error_setlasterror());
            }

            _ASSERT(*plock->prcount > 0);
            (*plock->prcount)--;

            if(*plock->prcount == 0)
            {
                SetEvent(plock->hcanwrite);
            }

            ReleaseMutex(plock->haccess);
            break;

        case LOCK_USET_XREAD_XWRITE:
            /* Simple case. Just release the mutex */
            ReleaseMutex(plock->hxwrite);
            break;

        default:
            _ASSERT(FALSE);
            break;
    }

    plock->state = LOCK_STATE_UNLOCKED;

    dprintverbose("end lock_readunlock");
    return;
}

/* Acquire write lock */
void lock_writelock(lock_context * plock)
{
    HANDLE hArray[3] = {0};
    DWORD  ret       = 0;

    dprintverbose("start lock_writelock");

    _ASSERT(plock          != NULL);
    _ASSERT(plock->hxwrite != NULL);

    switch( plock->usetype )
    {
        case LOCK_USET_SREAD_XWRITE:
            /* Get the lock on write handle */
            /* Wait for readers to go down to 0 */
            /* Block any more readers */
            _ASSERT(plock->prcount != NULL);

            hArray[0] = plock->haccess;
            hArray[1] = plock->hcanwrite;
            hArray[2] = plock->hxwrite;
            ret = WaitForMultipleObjects(3, hArray, TRUE, INFINITE);

            if (ret == WAIT_ABANDONED_0 || ret == (WAIT_ABANDONED_0 + 2))
            {
                char whichLock = (ret == WAIT_ABANDONED_0 ? 'A' : 'X');
                error_setlasterror();
                dprintcritical("lock_writelock: acquired abandoned mutex %s%c. Something bad happend in another process!", 
                                plock->nameprefix, whichLock);
                php_error(E_ERROR, "WINCACHE: lock_writelock: acquired abandoned mutex %s%c. Something bad happend in another process!",
                          plock->nameprefix, whichLock);
            }

            if (ret == WAIT_FAILED)
            {
                dprintcritical("lock_writelock: Failure waiting on shared lock %s* (%d). Something bad happened!", 
                                plock->nameprefix, error_setlasterror());
                php_error(E_ERROR, "WINCACHE: lock_writelock: Failure waiting on shared lock %s* (%d). Something bad happened!", 
                          plock->nameprefix, error_getlasterror());
            }

            _ASSERT(*plock->prcount == 0);

            ResetEvent(plock->hcanread);
            ReleaseMutex(plock->haccess);

            break;
        case LOCK_USET_XREAD_XWRITE:
            /* Simple case. Just lock hxwrite */
            ret = WaitForSingleObject(plock->hxwrite, INFINITE);

            if (ret == WAIT_ABANDONED)
            {
                error_setlasterror();
                dprintcritical("lock_writelock: acquired abandoned mutex %sX. Something bad happend in another process!",
                                plock->nameprefix);
                php_error(E_ERROR, "WINCACHE: lock_writelock: acquired abandoned mutex %sX. Something bad happend in another process!",
                                plock->nameprefix);
            }

            if (ret == WAIT_FAILED)
            {
                dprintcritical("lock_writelock: Failure waiting on lock %sX (%d). Something bad happened!", 
                                plock->nameprefix, error_setlasterror());
                php_error(E_ERROR, "WINCACHE: lock_writelock: Failure waiting on lock %sX (%d). Something bad happened!", 
                          plock->nameprefix, error_getlasterror());
            }

            break;

        default:
            _ASSERT(FALSE);
            break;
    }

    plock->state = LOCK_STATE_WRITELOCK;

    dprintverbose("end lock_writelock");
    return;
}

/* Release write lock */
void lock_writeunlock(lock_context * plock)
{
    DWORD  ret       = 0;

    dprintverbose("start lock_writeunlock");

    _ASSERT(plock          != NULL);
    _ASSERT(plock->hxwrite != NULL);

    switch(plock->usetype)
    {
        case LOCK_USET_SREAD_XWRITE:
            _ASSERT(plock->prcount != NULL);

            ret = WaitForSingleObject(plock->haccess, INFINITE);

            if (ret == WAIT_ABANDONED)
            {
                error_setlasterror();
                dprintcritical("lock_writeunlock: acquired abandoned mutex %sA. Something bad happend in another process!",
                                plock->nameprefix);
                php_error(E_ERROR, "WINCACHE: lock_writeunlock: acquired abandoned mutex %sA. Something bad happend in another process!",
                          plock->nameprefix);

                /* At this point, we know the reader count *must* be zero. */
                /* Let's take this opportunity to fix any problems */
                if (*plock->prcount != 0)
                {
                    dprintimportant("lock_writeunlock: invalid reader count detected (%d). Fixing...",
                                    *plock->prcount);
                    *plock->prcount = 0;
                }
            }

            if (ret == WAIT_FAILED)
            {
                dprintcritical("lock_writeunlock: Failure waiting on lock %sA (%d). Something bad happened!", 
                                plock->nameprefix, error_setlasterror());
                php_error(E_ERROR, "WINCACHE: lock_writeunlock: Failure waiting on lock %sA (%d). Something bad happened!", 
                          plock->nameprefix, error_getlasterror());
            }

            _ASSERT(*plock->prcount == 0);

            ReleaseMutex(plock->hxwrite);
            SetEvent(plock->hcanwrite);
            SetEvent(plock->hcanread);

            ReleaseMutex(plock->haccess);
            break;

        case LOCK_USET_XREAD_XWRITE:
            /* Simple case. Just release the mutex */
            ReleaseMutex(plock->hxwrite);
            break;

        default:
            _ASSERT(FALSE);
            break;
    }

    plock->state = LOCK_STATE_UNLOCKED;

    dprintverbose("end lock_writeunlock");
    return;
}

int lock_getnewname(lock_context * plock, char * suffix, char * newname, unsigned int length)
{
    int          result  = NONFATAL;
    unsigned int namelen = 0;
    unsigned int sufflen = 0;

    dprintverbose("start lock_getnewname");

    _ASSERT(plock             != NULL);
    _ASSERT(plock->nameprefix != NULL);
    _ASSERT(suffix            != NULL);
    _ASSERT(newname           != NULL);
    _ASSERT(length            >  0);

    sufflen = strlen(suffix);
    namelen = plock->namelen + sufflen + 1;

    if(length < namelen)
    {
        result = FATAL_LOCK_SHORT_BUFFER;
        goto Finished;
    }

    _snprintf_s(newname, length, length - 1, "%s%s", plock->nameprefix, suffix);
    newname[namelen] = 0;

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in lock_getnewname", result);
    }

    dprintverbose("end lock_getnewname");

    return result;
}

void lock_runtest()
{
    int            result  = NONFATAL;
    unsigned int   rdcount = 0;
    lock_context * plock1  = NULL;
    lock_context * plock2  = NULL;

    TSRMLS_FETCH();
    dprintverbose("*** STARTING LOCK TESTS ***");

    /* Create two locks of different types */
    result = lock_create(&plock1);
    if(FAILED(result))
    {
        dprintverbose("lock_create for plock1 failed");
        goto Finished;
    }

    result = lock_create(&plock2);
    if(FAILED(result))
    {
        dprintverbose("lock_create for plock2 failed");
        goto Finished;
    }

    /* Verify IDs of two locks are different */
    _ASSERT(plock1->id != plock2->id);

    /* Initialize first lock */
    result = lock_initialize(plock1, "LOCK_TEST1", 1, LOCK_TYPE_SHARED, LOCK_USET_SREAD_XWRITE, &rdcount TSRMLS_CC);
    if(FAILED(result))
    {
        dprintverbose("lock_initialize for plock1 failed");
        goto Finished;
    }

    /* Verify strings and handles got created properly */
    _ASSERT(plock1->namelen   == strlen(plock1->nameprefix));
    _ASSERT(plock1->haccess   != NULL);
    _ASSERT(plock1->hcanread  != NULL);
    _ASSERT(plock1->hcanwrite != NULL);
    _ASSERT(plock1->hxwrite   != NULL);

    /* Initialize second lock */
    result = lock_initialize(plock2, "LOCK_TEST2", 1, LOCK_TYPE_LOCAL, LOCK_USET_XREAD_XWRITE, NULL TSRMLS_CC);
    if(FAILED(result))
    {
        dprintverbose("lock_intialize for plock2 failed");
        goto Finished;
    }

    /* Verify strings and handles got created properly */
    _ASSERT(plock2->namelen   == strlen(plock2->nameprefix));
    _ASSERT(plock2->hxwrite   != NULL);
    _ASSERT(plock2->haccess   == NULL);
    _ASSERT(plock2->hcanread  == NULL);
    _ASSERT(plock2->hcanwrite == NULL);

    /* Get readlock on both locks */
    lock_readlock(plock1);
    lock_readlock(plock2);

    /* Verify reader count for shared read lock */
    _ASSERT(rdcount == 1);

    /* Get two more readlocks and verify reader count */
    lock_readlock(plock1);
    lock_readlock(plock1);

    _ASSERT(rdcount == 3);

    /* Decrement reader count and get write lock on lock2 */
    lock_readunlock(plock1);
    lock_readunlock(plock1);

    lock_readunlock(plock2);
    lock_writelock(plock2);

    _ASSERT(rdcount == 1);

    /* Get write lock on lock1 */
    lock_readunlock(plock1);
    lock_writelock(plock1);

    _ASSERT(rdcount == 0);

    lock_writeunlock(plock1);
    lock_writeunlock(plock2);

    /* Verify that after writelock, shared read is enabled again */
    lock_writelock(plock1);
    lock_writeunlock(plock1);

    /* TBD?? Add test which does read write in multiple threads */
    /* to test shared read, exclusive write properly */

    lock_readlock(plock1);
    lock_readlock(plock1);
    lock_readunlock(plock1);
    lock_readunlock(plock1);

    lock_writelock(plock1);
    lock_writeunlock(plock1);

    _ASSERT(rdcount == 0);

    _ASSERT(SUCCEEDED(result));

Finished:

    if(plock1 != NULL)
    {
        lock_terminate(plock1);
        lock_destroy(plock1);

        plock1 = NULL;
    }

    if(plock2 != NULL)
    {
        lock_terminate(plock2);
        lock_destroy(plock2);

        plock2 = NULL;
    }

    dprintverbose("*** ENDING LOCK TESTS ***");

    return;
}
