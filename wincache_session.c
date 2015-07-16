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
   | Module: wincache_session.c                                                                   |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

#define ZVSCACHE_KEY    1

ps_module ps_mod_wincache = { PS_MOD(wincache) };

static void scache_destructor(void * pdestination)
{
    zvcache_context ** ppcache = NULL;

    _ASSERT(pdestination != NULL);
    ppcache = (zvcache_context **)pdestination;

    if (WCG(zvscache) && WCG(zvscache) == *ppcache)
    {
        WCG(zvscache) = NULL;
    }

    zvcache_terminate(*ppcache);
    zvcache_destroy(*ppcache);

    ppcache = NULL;
    return;
}

/* Called on session start. Nothing to do */
PS_OPEN_FUNC(wincache)
{
    int                result     = NONFATAL;
    zvcache_context ** ppcache    = NULL;
    zvcache_context *  pzcache    = NULL;
    zend_ini_entry *   pinientry  = NULL;
    HashTable *        phtable    = NULL;
    unsigned char      hashupdate = 0;
    char *             scolon     = NULL;
    char  *            filepath   = NULL;
    unsigned int       fpathlen   = 0;
    unsigned int       cachekey   = 0;
    int                rethash    = 0;
    HANDLE             hOriginalToken = NULL;
    int                fCreatedHashtable = 0;

    dprintverbose("start ps_open_func");

    if(WCG(inisavepath) == NULL)
    {
        rethash = zend_hash_find(EG(ini_directives), "session.save_path", sizeof("session.save_path"), (void **)&pinientry);
        _ASSERT(rethash != FAILURE);
        WCG(inisavepath) = pinientry;
    }

    /* Create hashtable if not created yet */
    if(WCG(phscache) == NULL)
    {
        phtable = alloc_pemalloc(sizeof(HashTable));
        if(phtable == NULL)
        {
            result = FATAL_OUT_OF_LMEMORY;
            goto Finished;
        }

        zend_hash_init(phtable, 0, NULL, scache_destructor, 1);
        WCG(phscache) = phtable;
        phtable = NULL;
        fCreatedHashtable = 1;
    }

    /* Use zvscache for unmodified save_path. Else get zvcache_context from phscache */
    /* If save path is modified but is same as PHP_INI_SYSTEM, use zvscache */
    if(WCG(inisavepath)->modified == 0 || _stricmp(WCG(inisavepath)->orig_value, WCG(inisavepath)->value) == 0)
    {
        pzcache  = WCG(zvscache);
        cachekey = ZVSCACHE_KEY;
        hashupdate = 1;
    }
    else
    {
        /* If save path is modified and is not blank, use its own session cache */
        /* savepath specific cache key minimum is 2 and maximum is 65535 */
        cachekey = utils_hashcalc(save_path, strlen(save_path));
        cachekey = (cachekey % 65534) + 2;

        if(zend_hash_index_find(WCG(phscache), (ulong)cachekey, (void **)&ppcache) == FAILURE)
        {
            /* If cachekey cache is not found, update it after creating it */
            hashupdate = 1;
        }
        else
        {
            pzcache = *ppcache;
        }
    }

    /* If session cache is not created yet create now */
    /* Handling save_path as is done in ext\session\mod_files.c */
    if(pzcache == NULL)
    {
        result = utils_revert_if_necessary(&hOriginalToken);
        if (FAILED(result))
        {
            goto Finished;
        }

        if(*save_path != '\0')
        {
            /* Get last portion from [dirdepth;[filemode;]]dirpath */
            scolon = strchr(save_path, ';');
            while(scolon != NULL)
            {
                save_path = scolon + 1;
                scolon = strchr(save_path, ';');
            }
        }

        /* Use temporary folder as save_path if its blank */
        if(*save_path == '\0')
        {
            save_path = php_get_temporary_directory();

            /* Check if path is accessible as per open_basedir */
            if(
#ifndef ZEND_ENGINE_2_4
                (PG(safe_mode) && (!php_checkuid(save_path, NULL, CHECKUID_CHECK_FILE_AND_DIR))) ||
#endif /* ZEND_ENGINE_2_4 */
                php_check_open_basedir(save_path TSRMLS_CC))
            {
                result = FATAL_SESSION_INITIALIZE;
                goto Finished;
            }
        }

        _ASSERT(save_path != NULL);

        /* Create path as save_path\wincache_<version>_session_[<namesalt>_]ppid. 6 for _<cachekey> */
        fpathlen = strlen(save_path) + 1 + sizeof("wincache__session_.tmp") + 6 + ((WCG(namesalt) == NULL) ? 0 : (strlen(WCG(namesalt)) + 1)) + 5;

        /* add the PHP_AND_WINCACHE_VERSION */
        fpathlen += sizeof(STRVER2(PHP_MAJOR_VERSION, PHP_MINOR_VERSION));
        fpathlen += PHP_WINCACHE_VERSION_LEN;

        if(fpathlen > MAX_PATH)
        {
            result = FATAL_SESSION_PATHLONG;
            goto Finished;
        }

        filepath = (char *)alloc_pemalloc(fpathlen);
        if(filepath == NULL)
        {
            result = FATAL_OUT_OF_LMEMORY;
            goto Finished;
        }

        ZeroMemory(filepath, fpathlen);
        if(WCG(namesalt) == NULL)
        {
            _snprintf_s(filepath, fpathlen, fpathlen - 1, "%s\\wincache_" STRVER2(PHP_MAJOR_VERSION, PHP_MINOR_VERSION) "_" PHP_WINCACHE_VERSION "_session_%u_%u.tmp", save_path, cachekey, WCG(parentpid));
        }
        else
        {
            _snprintf_s(filepath, fpathlen, fpathlen - 1, "%s\\wincache_" STRVER2(PHP_MAJOR_VERSION, PHP_MINOR_VERSION) "_" PHP_WINCACHE_VERSION "_session_%u_%s_%u.tmp", save_path, cachekey, WCG(namesalt), WCG(parentpid));
        }

        /* Create session cache on call to session_start */
        result = zvcache_create(&pzcache);
        if(FAILED(result))
        {
            goto Finished;
        }

        /* issession = 1, islocal = 0, zvcount = 256 */
        result = zvcache_initialize(pzcache, 1, 0, (unsigned short)cachekey, 256, WCG(scachesize), filepath TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        if(hashupdate)
        {
            zend_hash_index_update(WCG(phscache), (ulong)cachekey, (void **)&pzcache, sizeof(zvcache_context *), NULL);
        }

        if(cachekey == ZVSCACHE_KEY)
        {
            WCG(zvscache) = pzcache;
        }
    }

    PS_SET_MOD_DATA(pzcache);

Finished:

    utils_reimpersonate_if_necessary(hOriginalToken);
    hOriginalToken = NULL;

    if(filepath != NULL)
    {
        alloc_pefree(filepath);
        filepath = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in ps_open_func", result);

        if (fCreatedHashtable)
        {
            /*
             * If we failed after creating the hashtable and assigning it to
             * WCG(phscache), then we need to clear out WCG(phscache).
             */
            phtable = WCG(phscache);
            WCG(phscache) = NULL;
        }

        if(phtable != NULL)
        {
            zend_hash_destroy(phtable);
            alloc_pefree(phtable);
            phtable = NULL;
        }

        if(pzcache != NULL)
        {
            if (WCG(zvscache) == pzcache)
            {
                WCG(zvscache) = NULL;
            }

            /*
             * NOTE: It should be safe to destroy the pzcache at this point,
             * since it should NOT have been added to the phtable.  If it had,
             * this would lead to a double-deref of the pzcache.
             */
            zvcache_terminate(pzcache);
            zvcache_destroy(pzcache);

            pzcache = NULL;
        }

        dprintverbose("end ps_open_func");
        return FAILURE;
    }

    dprintverbose("end ps_open_func");
    return SUCCESS;
}

/* Called on session close.  Nothing to do. */
PS_CLOSE_FUNC(wincache)
{
    dprintverbose("start ps_close_func");

    PS_SET_MOD_DATA(NULL);

    dprintverbose("end ps_close_func");
    return SUCCESS;
}

/* Called on session start which reads all values into memory */
PS_READ_FUNC(wincache)
{
    int               result  = NONFATAL;
    zval *            pzval   = NULL;
    zvcache_context * pzcache = NULL;

    dprintverbose("start ps_read_func");

    _ASSERT(key    != NULL);
    _ASSERT(val    != NULL);
    _ASSERT(vallen != NULL);

    pzcache = PS_GET_MOD_DATA();
    if(pzcache == NULL)
    {
        result = FATAL_SESSION_INITIALIZE;
        goto Finished;
    }

    MAKE_STD_ZVAL(pzval);
    ZVAL_NULL(pzval);

    result = zvcache_get(pzcache, key, &pzval TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    *val = Z_STRVAL_P(pzval);
    *vallen = Z_STRLEN_P(pzval);

Finished:

    if(pzval != NULL)
    {
        FREE_ZVAL(pzval);
        pzval = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in ps_read_func", result);


        return FAILURE;
    }

    dprintverbose("end ps_read_func");
    return SUCCESS;
}

/* Called on session close which writes all values to memory */
PS_WRITE_FUNC(wincache)
{
    int               result  = NONFATAL;
    zval *            pzval   = NULL;
    zvcache_context * pzcache = NULL;

    dprintverbose("start ps_write_func");

    _ASSERT(key != NULL);
    _ASSERT(val != NULL);

    pzcache = PS_GET_MOD_DATA();
    if(pzcache == NULL)
    {
        result = FATAL_SESSION_INITIALIZE;
        goto Finished;
    }

    MAKE_STD_ZVAL(pzval);
    ZVAL_STRINGL(pzval, val, vallen, 0);

    /* ttl = session.gc_maxlifetime, isadd = 0 */
    result = zvcache_set(pzcache, key, pzval, INI_INT("session.gc_maxlifetime"), 0 TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

Finished:

    if(pzval != NULL)
    {
        FREE_ZVAL(pzval);
        pzval = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in ps_write_func", result);

        return FAILURE;
    }

    dprintverbose("end ps_write_func");
    return SUCCESS;
}

/* Called on session_destroy */
PS_DESTROY_FUNC(wincache)
{
    int               result  = NONFATAL;
    zvcache_context * pzcache = NULL;

    dprintverbose("start ps_destroy_func");

    _ASSERT(key != NULL);

    pzcache = PS_GET_MOD_DATA();
    if(pzcache == NULL)
    {
        result = FATAL_SESSION_INITIALIZE;
        goto Finished;
    }

    result = zvcache_delete(pzcache, key);
    if(FAILED(result))
    {
        /* Entry not found is not a fatal error */
        if(result = WARNING_ZVCACHE_EMISSING)
        {
            result = NONFATAL;
        }

        goto Finished;
    }

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in ps_destroy_func", result);

        return FAILURE;
    }

    dprintverbose("end ps_destroy_func");
    return SUCCESS;
}

/* Do garbage collection of entries which are older */
/* than the max life time maintained by session extension */
PS_GC_FUNC(wincache)
{
    dprintverbose("start ps_gc_func");

    dprintverbose("end ps_gc_func");
    return SUCCESS;
}
