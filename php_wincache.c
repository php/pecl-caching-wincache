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
   | Module: php_wincache.c                                                                       |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

#define NUM_FILES_MINIMUM           1024
#define NUM_FILES_MAXIMUM           16384
#define OCACHE_SIZE_MINIMUM         32
#define OCACHE_SIZE_MAXIMUM         256
#define FCACHE_SIZE_MINIMUM         32
#define FCACHE_SIZE_MAXIMUM         256
#define FILE_SIZE_MINIMUM           10
#define FILE_SIZE_MAXIMUM           2048
#define FCHECK_FREQ_MINIMUM         2
#define FCHECK_FREQ_MAXIMUM         300
#define TTL_VALUE_MINIMUM           60
#define TTL_VALUE_MAXIMUM           7200

#define NAMESALT_LENGTH_MAXIMUM     8
#define OCENABLED_INDEX_IN_GLOBALS  1
#define WINCACHE_TEST_FILE          "wincache.php"

/* START OF PHP EXTENSION MACROS STUFF */
PHP_MINIT_FUNCTION(wincache);
PHP_MSHUTDOWN_FUNCTION(wincache);
PHP_RINIT_FUNCTION(wincache);
PHP_RSHUTDOWN_FUNCTION(wincache);
PHP_MINFO_FUNCTION(wincache);

/* Info functions exposed by this extension */
PHP_FUNCTION(wincache_rplist_fileinfo);
PHP_FUNCTION(wincache_rplist_meminfo);
PHP_FUNCTION(wincache_fcache_fileinfo);
PHP_FUNCTION(wincache_fcache_meminfo);
PHP_FUNCTION(wincache_ocache_fileinfo);
PHP_FUNCTION(wincache_ocache_meminfo);

/* Utility functions exposed by this extension */
PHP_FUNCTION(wincache_refresh_if_changed);

#ifdef WINCACHE_TEST
PHP_FUNCTION(wincache_runtests);
PHP_FUNCTION(wincache_fcache_find);
PHP_FUNCTION(wincache_ocache_find);
#endif

ZEND_DECLARE_MODULE_GLOBALS(wincache)

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_rplist_fileinfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_rplist_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_fcache_fileinfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_fcache_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ocache_fileinfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ocache_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_refresh_if_changed, 0, 0, 0)
    ZEND_ARG_INFO(0, file_list)
ZEND_END_ARG_INFO()

#ifdef WINCACHE_TEST
ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_runtests, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_fcache_find, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ocache_find, 0, 0, 3)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, searchitem)
    ZEND_ARG_INFO(0, scope)
ZEND_END_ARG_INFO()
#endif
        
/* Put all user defined functions here */
function_entry wincache_functions[] = {
    PHP_FE(wincache_rplist_fileinfo, arginfo_wincache_rplist_fileinfo)
    PHP_FE(wincache_rplist_meminfo, arginfo_wincache_rplist_meminfo)
    PHP_FE(wincache_fcache_fileinfo, arginfo_wincache_fcache_fileinfo)
    PHP_FE(wincache_fcache_meminfo, arginfo_wincache_fcache_meminfo)
    PHP_FE(wincache_ocache_fileinfo, arginfo_wincache_ocache_fileinfo)
    PHP_FE(wincache_ocache_meminfo, arginfo_wincache_ocache_meminfo)
    PHP_FE(wincache_refresh_if_changed, arginfo_wincache_refresh_if_changed)
#ifdef WINCACHE_TEST
    PHP_FE(wincache_runtests, arginfo_wincache_runtests)
    PHP_FE(wincache_fcache_find, arginfo_wincache_fcache_find)
    PHP_FE(wincache_ocache_find, arginfo_wincache_ocache_find)
#endif
    {NULL, NULL, NULL}
};

/* wincache_module_entry */
zend_module_entry wincache_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_WINCACHE_EXTNAME,
    wincache_functions,      /* Functions */
    PHP_MINIT(wincache),     /* MINIT */
    PHP_MSHUTDOWN(wincache), /* MSHUTDOWN */
    PHP_RINIT(wincache),     /* RINIT */
    PHP_RSHUTDOWN(wincache), /* RSHUTDOWN */
    PHP_MINFO(wincache),     /* MINFO */
#if ZEND_MODULE_API_NO >= 20010901
    PHP_WINCACHE_EXTVER,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_WINCACHE
    ZEND_GET_MODULE(wincache)
#endif

/* If index of ocenabled is changed, change the define statement at top */
PHP_INI_BEGIN()
/* index 0 */  STD_PHP_INI_BOOLEAN("wincache.fcenabled", "1", PHP_INI_ALL, OnUpdateBool, fcenabled, zend_wincache_globals, wincache_globals)
/* index 1 */  STD_PHP_INI_BOOLEAN("wincache.ocenabled", "1", PHP_INI_ALL, OnUpdateBool, ocenabled, zend_wincache_globals, wincache_globals)
/* index 2 */  STD_PHP_INI_BOOLEAN("wincache.enablecli", "0", PHP_INI_SYSTEM, OnUpdateBool, enablecli, zend_wincache_globals, wincache_globals)
/* index 3 */  STD_PHP_INI_ENTRY("wincache.fcachesize", "128", PHP_INI_SYSTEM, OnUpdateLong, fcachesize, zend_wincache_globals, wincache_globals)
/* index 4 */  STD_PHP_INI_ENTRY("wincache.ocachesize", "128", PHP_INI_SYSTEM, OnUpdateLong, ocachesize, zend_wincache_globals, wincache_globals)
/* index 5 */  STD_PHP_INI_ENTRY("wincache.maxfilesize", "256", PHP_INI_SYSTEM, OnUpdateLong, maxfilesize, zend_wincache_globals, wincache_globals)
/* index 6 */  STD_PHP_INI_ENTRY("wincache.filecount", "4096", PHP_INI_SYSTEM, OnUpdateLong, numfiles, zend_wincache_globals, wincache_globals)
/* index 7 */  STD_PHP_INI_ENTRY("wincache.chkinterval", "30", PHP_INI_SYSTEM, OnUpdateLong, fcchkfreq, zend_wincache_globals, wincache_globals)
/* index 8 */  STD_PHP_INI_ENTRY("wincache.ttlmax", "1200", PHP_INI_SYSTEM, OnUpdateLong, ttlmax, zend_wincache_globals, wincache_globals)
/* index 9 */  STD_PHP_INI_ENTRY("wincache.debuglevel", "0", PHP_INI_SYSTEM, OnUpdateLong, debuglevel, zend_wincache_globals, wincache_globals)
/* index 10 */ STD_PHP_INI_ENTRY("wincache.ignorelist", NULL, PHP_INI_ALL, OnUpdateString, ignorelist, zend_wincache_globals, wincache_globals)
/* index 11 */ STD_PHP_INI_ENTRY("wincache.ocenabledfilter", NULL, PHP_INI_SYSTEM, OnUpdateString, ocefilter, zend_wincache_globals, wincache_globals)
/* index 12 */ STD_PHP_INI_ENTRY("wincache.namesalt", NULL, PHP_INI_SYSTEM, OnUpdateString, namesalt, zend_wincache_globals, wincache_globals)
/* index 13 */ STD_PHP_INI_ENTRY("wincache.localheap", "0", PHP_INI_SYSTEM, OnUpdateBool, localheap, zend_wincache_globals, wincache_globals)
PHP_INI_END()

/* END OF PHP EXTENSION MACROS STUFF */

static void globals_initialize(zend_wincache_globals * globals TSRMLS_DC);
static void globals_terminate(zend_wincache_globals * globals TSRMLS_DC);
static unsigned char isin_ignorelist(const char * ignorelist, const char * filename);
static unsigned char isin_cseplist(const char * cseplist, const char * szinput);
static void errmsglist_dtor(void * pvoid);

/* Initialize globals */
static void globals_initialize(zend_wincache_globals * globals TSRMLS_DC)
{
    dprintverbose("start globals_initialize");

    WCG(fcenabled)   = 1;    /* File cache enabled by default */
    WCG(ocenabled)   = 1;    /* Opcode cache enabled by default */
    WCG(enablecli)   = 0;    /* WinCache not enabled by default for CLI */
    WCG(fcachesize)  = 128;  /* File cache size is 128 MB by default */
    WCG(ocachesize)  = 128;  /* Opcode cache size is 128 MB by default */
    WCG(maxfilesize) = 256;  /* Maximum file size to cache is 256 KB */
    WCG(numfiles)    = 4096; /* 4096 hashtable keys */
    WCG(fcchkfreq)   = 30;   /* File change check done every 30 secs */
    WCG(ttlmax)      = 1200; /* File removed if not used for 1200 secs */
    WCG(debuglevel)  = 0;    /* Debug dump not enabled by default */
    WCG(ignorelist)  = NULL; /* List of files to ignore for caching */
    WCG(ocefilter)   = NULL; /* List of sites for which ocenabled is toggled */
    WCG(namesalt)    = NULL; /* Salt to use in names used by wincache */
    WCG(localheap)   = 0;    /* Local heap is disabled by default */

    WCG(lasterror)   = 0;    /* GetLastError() value set by wincache */
    WCG(lfcache)     = NULL; /* Aplist to use for file/rpath cache */
    WCG(locache)     = NULL; /* Aplist to use for opcode cache */
    WCG(issame)      = 1;    /* Indicates if same aplist is used */
    WCG(oclisthead)  = NULL; /* Head of in-use ocache values list */
    WCG(oclisttail)  = NULL; /* Tail of in-use ocache values list */
    WCG(parentpid)   = 0;    /* Parent process identifier to use */
    WCG(fmapgdata)   = NULL; /* Global filemap information data */
    WCG(errmsglist)  = NULL; /* Error messages list generated by PHP */
    WCG(dooctoggle)  = 0;    /* If set to 1, toggle value of ocenabled */

    dprintverbose("end globals_initialize");
    return;
}

/* Terminate globals */
static void globals_terminate(zend_wincache_globals * globals TSRMLS_DC)
{
    dprintverbose("start globals_terminate");
    dprintverbose("end globals_terminate");

    return;
}

static unsigned char isin_ignorelist(const char * ignorelist, const char * filename)
{
    unsigned char retvalue   = 0;
    char *        searchstr  = NULL;
    char *        fslash     = NULL;
    char          filestr[     MAX_PATH];
    char          tempchar   = 0;
    unsigned int  length     = 0;

    dprintverbose("start isin_ignorelist");

    _ASSERT(filename != NULL);

    if(ignorelist == NULL)
    {
        goto Finished;
    }

    /* Get the file name portion from filename */
    searchstr = strrchr(filename, '\\');
    if(searchstr != NULL)
    {
        fslash = strrchr(filename, '/');
        if(fslash > searchstr)
        {
            filename = fslash + 1;
        }
        else
        {
            filename = searchstr + 1;
        }
    }
    else
    {
        searchstr = strrchr(filename, '/');
        if(searchstr != NULL)
        {
            filename = searchstr + 1;
        }
    }

    length = strlen(filename);
    if(length == 0 || length > MAX_PATH - 3)
    {
        goto Finished;
    }

    /* filestr is "|filename|" */
    ZeroMemory(filestr, MAX_PATH);

    filestr[0] = '|';
    strncpy(filestr + 1, filename, length);
    _strlwr(filestr);

    /* Check if filename exactly matches ignorelist */
    /* Both are lowercase and case sensitive lookup is fine */
    if(strcmp(ignorelist, filestr + 1) == 0)
    {
        retvalue = 1;
        goto Finished;
    }

    /* Check if filename is in end or middle of ignorelist */
    searchstr = strstr(ignorelist, filestr);
    if(searchstr != NULL)
    {
        tempchar = *(searchstr + length + 1);
        if(tempchar == '|' || tempchar == 0)
        {
            retvalue = 1;
            goto Finished;
        }
    }

    /* Check if filename is in the start of ignorelist */
    filestr[length + 1] = '|';
    searchstr = strstr(ignorelist, filestr + 1);
    if(searchstr != NULL && searchstr == ignorelist)
    {
        retvalue = 1;
        goto Finished;
    }

Finished:

    dprintverbose("end isin_ignorelist");

    return retvalue;
}

static unsigned char isin_cseplist(const char * cseplist, const char * szinput)
{
    unsigned char retvalue   = 0;
    char *        searchstr  = NULL;
    char          inputstr[    MAX_PATH];
    char          tempchar   = 0;
    unsigned int  length     = 0;

    dprintverbose("start isin_cseplist");

    _ASSERT(szinput != NULL);

    /* If list is NULL or input length is 0, return false */
    if(cseplist == NULL)
    {
        goto Finished;
    }

    length = strlen(szinput);
    if(length == 0)
    {
        goto Finished;
    }

    /* inputstr is ",szinput," */
    ZeroMemory(inputstr, MAX_PATH);

    inputstr[0] = ',';
    strncpy(inputstr + 1, szinput, length);
    _strlwr(inputstr);

    /* Check if szinput exactly matches cseplist */
    /* Both are lowercase and case sensitive lookup is fine */
    if(strcmp(cseplist, inputstr + 1) == 0)
    {
        retvalue = 1;
        goto Finished;
    }

    /* Check if szinput is in end or middle of cseplist */
    searchstr = strstr(cseplist, inputstr);
    if(searchstr != NULL)
    {
        tempchar = *(searchstr + length + 1);
        if(tempchar == ',' || tempchar == 0)
        {
            retvalue = 1;
            goto Finished;
        }
    }

    /* Check if szinput is in the start of cseplist */
    inputstr[length + 1] = ',';
    searchstr = strstr(cseplist, inputstr + 1);
    if(searchstr != NULL && searchstr == cseplist)
    {
        retvalue = 1;
        goto Finished;
    }

Finished:

    dprintverbose("end isin_cseplist");

    return retvalue;
}

PHP_MINIT_FUNCTION(wincache)
{
    int              result    = NONFATAL;
    aplist_context * plcache1  = NULL;
    aplist_context * plcache2  = NULL;
    unsigned short   islocal   = 0;
    int              resnumber = -1;
    zend_extension   extension = {0};

    ZEND_INIT_MODULE_GLOBALS(wincache, globals_initialize, globals_terminate);
    REGISTER_INI_ENTRIES();

    /* Set the debuglevel as configured in globals/ini */
    dprintsetlevel(WCG(debuglevel));
    dprintverbose("start php_minit");

    /* If enablecli is set to 0, don't initialize when run with cli sapi */
    if(!WCG(enablecli) && !strcmp(sapi_module.name, "cli"))
    {
        goto Finished;
    }

    /* Compare value of globals with minimum and maximum allowed */
    WCG(numfiles)    = (WCG(numfiles)    < NUM_FILES_MINIMUM)   ? NUM_FILES_MINIMUM   : WCG(numfiles);
    WCG(numfiles)    = (WCG(numfiles)    > NUM_FILES_MAXIMUM)   ? NUM_FILES_MAXIMUM   : WCG(numfiles);
    WCG(ocachesize)  = (WCG(ocachesize)  < OCACHE_SIZE_MINIMUM) ? OCACHE_SIZE_MINIMUM : WCG(ocachesize);
    WCG(ocachesize)  = (WCG(ocachesize)  > OCACHE_SIZE_MAXIMUM) ? OCACHE_SIZE_MAXIMUM : WCG(ocachesize);
    WCG(fcachesize)  = (WCG(fcachesize)  < FCACHE_SIZE_MINIMUM) ? FCACHE_SIZE_MINIMUM : WCG(fcachesize);
    WCG(fcachesize)  = (WCG(fcachesize)  > FCACHE_SIZE_MAXIMUM) ? FCACHE_SIZE_MAXIMUM : WCG(fcachesize);
    WCG(maxfilesize) = (WCG(maxfilesize) < FILE_SIZE_MINIMUM)   ? FILE_SIZE_MINIMUM   : WCG(maxfilesize);
    WCG(maxfilesize) = (WCG(maxfilesize) > FILE_SIZE_MAXIMUM)   ? FILE_SIZE_MAXIMUM   : WCG(maxfilesize);
    WCG(ttlmax)      = (WCG(ttlmax)      < TTL_VALUE_MINIMUM)   ? TTL_VALUE_MINIMUM   : WCG(ttlmax);
    WCG(ttlmax)      = (WCG(ttlmax)      > TTL_VALUE_MAXIMUM)   ? TTL_VALUE_MAXIMUM   : WCG(ttlmax);

    /* fcchkfreq can be set to 0 which will mean check is completely disabled */
    if(WCG(fcchkfreq) != 0)
    {
        WCG(fcchkfreq)   = (WCG(fcchkfreq)   < FCHECK_FREQ_MINIMUM) ? FCHECK_FREQ_MINIMUM : WCG(fcchkfreq);
        WCG(fcchkfreq)   = (WCG(fcchkfreq)   > FCHECK_FREQ_MAXIMUM) ? FCHECK_FREQ_MAXIMUM : WCG(fcchkfreq);
    }

    /* Truncate namesalt to 8 characters */
    if(WCG(namesalt) != NULL && strlen(WCG(namesalt)) > NAMESALT_LENGTH_MAXIMUM)
    {
        *(WCG(namesalt) + NAMESALT_LENGTH_MAXIMUM) = 0;
    }

    /* Convert ignorelist to lowercase soon enough */
    if(WCG(ignorelist) != NULL)
    {
        _strlwr(WCG(ignorelist));
    }

    /* Even if enabled is set to false, create cache and set */
    /* the hook because scripts can selectively enable it */

    /* Call filemap global initialized. Terminate in MSHUTDOWN */
    result = filemap_global_initialize(TSRMLS_C);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Create filelist cache */
    result = aplist_create(&plcache1);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = aplist_initialize(plcache1, islocal, WCG(numfiles), WCG(fcchkfreq), WCG(ttlmax) TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = aplist_fcache_initialize(plcache1, WCG(fcachesize), WCG(maxfilesize) TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    WCG(lfcache) = plcache1;

    original_stream_open_function = zend_stream_open_function;
    zend_stream_open_function = wincache_stream_open_function;

#ifndef PHP_VERSION_52
    original_resolve_path = zend_resolve_path;
    zend_resolve_path = wincache_resolve_path;
#endif

    dprintverbose("Installed function hooks for stream_open");

    resnumber = zend_get_resource_handle(&extension);
    if(resnumber == -1)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    result = aplist_ocache_initialize(plcache1, resnumber, WCG(ocachesize) TSRMLS_CC);
    if(FAILED(result))
    {
        if(result != FATAL_FILEMAP_MAPVIEW)
        {
            goto Finished;
        }

        /* Couldn't map opcode cache segment at same address */
        /* Create a local copy instead */
        plcache1 = NULL;
        islocal  = 1;

        result = aplist_create(&plcache2);
        if(FAILED(result))
        {
            goto Finished;
        }

        result = aplist_initialize(plcache2, islocal, WCG(numfiles), WCG(fcchkfreq), WCG(ttlmax) TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        result = aplist_ocache_initialize(plcache2, resnumber, WCG(ocachesize) TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        WCG(locache) = plcache2;
        WCG(issame)  = 0;
    }
    else
    {
        WCG(locache) = plcache1;
    }

    original_compile_file = zend_compile_file;
    zend_compile_file = wincache_compile_file;

    /* Also keep zend_error_cb pointer for saving generating messages */
    original_error_cb = zend_error_cb;

    dprintverbose("Installed function hooks for compile_file");

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in php_minit", result);
        _ASSERT(FALSE);

        if(plcache1 != NULL)
        {
            aplist_terminate(plcache1);
            aplist_destroy(plcache1);

            plcache1 = NULL;
        }

        if(plcache2 != NULL)
        {
            aplist_terminate(plcache2);
            aplist_destroy(plcache2);

            plcache2 = NULL;
        }
    }

    dprintverbose("end php_minit");
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(wincache)
{
    dprintverbose("start php_mshutdown");

    /* If enablecli is 0, short circuit this call in cli mode */
    if(!WCG(enablecli) && !strcmp(sapi_module.name, "cli"))
    {
        goto Finished;
    }
    
#ifndef PHP_VERSION_52
    zend_resolve_path = original_resolve_path;
#endif

    zend_stream_open_function = original_stream_open_function;
    zend_compile_file = original_compile_file;
    
    if(WCG(lfcache) != NULL)
    {
        aplist_terminate(WCG(lfcache));
        aplist_destroy(WCG(lfcache));

        WCG(lfcache) = NULL;
    }

    if(!WCG(issame) && WCG(locache) != NULL)
    {
        aplist_terminate(WCG(locache));
        aplist_destroy(WCG(locache));

        WCG(locache) = NULL;
    }

    filemap_global_terminate(TSRMLS_C);

Finished: 

    /* Unregister ini entries. globals_terminate will get */
    /* called by zend engine after this */
    UNREGISTER_INI_ENTRIES();

    dprintverbose("end php_mshutdown");
    return SUCCESS;
}

PHP_RINIT_FUNCTION(wincache)
{
    zval ** siteid = NULL;

    /* If enablecli is 0, short circuit this call in cli mode */
    if(!WCG(enablecli) && !strcmp(sapi_module.name, "cli"))
    {
        goto Finished;
    }

    _ASSERT(WCG(oclisthead) == NULL);
    _ASSERT(WCG(oclisttail) == NULL);

    if(WCG(ocefilter) != NULL)
    {
        /* Read site id from list of variables */
        zend_is_auto_global("_SERVER", sizeof("_SERVER") - 1 TSRMLS_CC);

        if (!PG(http_globals)[TRACK_VARS_SERVER] ||
            zend_hash_find(PG(http_globals)[TRACK_VARS_SERVER]->value.ht, "INSTANCE_ID", sizeof("INSTANCE_ID"), (void **) &siteid) == FAILURE)
        {
            goto Finished;
        }

        if(isin_cseplist(WCG(ocefilter), Z_STRVAL_PP(siteid)))
        {
            WCG(dooctoggle) = 1;
        }
    }

    /* zend_error_cb needs to be wincache_error_cb only when original_compile_file is used */
    zend_error_cb = original_error_cb;

Finished:

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(wincache)
{
    ocacheval_list * poentry = NULL;
    ocacheval_list * potemp  = NULL;

    /* If enablecli is 0, short circuit this call in cli mode */
    if(!WCG(enablecli) && !strcmp(sapi_module.name, "cli"))
    {
        goto Finished;
    }

    /* If we picked the entry from cache, decrement */
    /* refcount to tell that its use is over */
    poentry = WCG(oclisthead);
    while(poentry != NULL)
    {
        potemp  = poentry;
        poentry = poentry->next;

        _ASSERT(potemp->pvalue != NULL);

        aplist_ocache_close(WCG(locache), potemp->pvalue);
        alloc_efree(potemp);
        potemp = NULL;
    }

    /* Destroy errmsglist now before zend_mm does it */
    if(WCG(errmsglist) != NULL)
    {
        zend_llist_destroy(WCG(errmsglist));
        alloc_efree(WCG(errmsglist));
        WCG(errmsglist) = NULL;
    }

    WCG(oclisthead) = NULL;
    WCG(oclisttail) = NULL;
    WCG(dooctoggle) = 0;

Finished:

    return SUCCESS;
}

PHP_MINFO_FUNCTION(wincache)
{
    dprintverbose("start php_minfo");

    php_info_print_table_start();

    if(!WCG(enablecli) && !strcmp(sapi_module.name, "cli"))
    {
        php_info_print_table_row(2, "Opcode cache", "disabled");
        php_info_print_table_row(2, "File cache", "disabled");
    }
    else
    {
        php_info_print_table_row(2, "Opcode cache", WCG(ocenabled) ? "enabled" : "disabled");
        php_info_print_table_row(2, "File cache", WCG(fcenabled) ? "enabled" : "disabled");
    }

    php_info_print_table_row(2, "Version", PHP_WINCACHE_EXTVER);
    php_info_print_table_row(2, "Owner", "iisphp@microsoft.com");
    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);

    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();

    dprintverbose("end php_minfo");
    return;
}

char * wincache_resolve_path(const char * filename, int filename_len TSRMLS_DC)
{
    int    result       = NONFATAL;
    char * resolve_path = NULL;

#ifdef PHP_VERSION_52
    dprintimportant("wincache_resolve_path shouldn't get called for 5.2.X");
    _ASSERT(FALSE);
#endif

    /* If wincache.fcenabled is set to 0 but some how */
    /* this method is called, use original_resolve_path */
    if(!WCG(fcenabled) || filename == NULL)
    {
        return original_resolve_path(filename, filename_len TSRMLS_CC);
    }

    dprintimportant("zend_resolve_path called for %s", filename);

    /* Hook so that test file is not added to cache */
    if(strstr(filename, WINCACHE_TEST_FILE) != NULL || isin_ignorelist(WCG(ignorelist), filename))
    {
        dprintimportant("cache is disabled for the file because of ignore list");
        return original_resolve_path(filename, filename_len TSRMLS_CC);
    }

    /* Keep last argument as NULL to indicate that we only want fullpath of file */
    result = aplist_fcache_get(WCG(lfcache), filename, &resolve_path, NULL TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("wincache_resolve_path failed with error %u", result);
        _ASSERT(FALSE);

        if(resolve_path != NULL)
        {
            alloc_efree(resolve_path);
            resolve_path = NULL;
        }

        return original_resolve_path(filename, filename_len TSRMLS_CC);
    }

    return resolve_path;
}

int wincache_stream_open_function(const char * filename, zend_file_handle * file_handle TSRMLS_DC)
{
    int            result   = NONFATAL;
    fcache_value * pfvalue  = NULL;
    char *         fullpath = NULL;

    /* If wincache.fcenabled is set to 0 but some how */
    /* this method is called, use original_stream_open_function */
    if(!WCG(fcenabled) || filename == NULL)
    {
        return original_stream_open_function(filename, file_handle TSRMLS_CC);
    }

    dprintimportant("zend_stream_open_function called for %s", filename);

    /* Hook so that test file is not added to cache */
    if(strstr(filename, WINCACHE_TEST_FILE) != NULL || isin_ignorelist(WCG(ignorelist), filename))
    {
        dprintimportant("cache is disabled for the file because of ignore list");
        return original_stream_open_function(filename, file_handle TSRMLS_CC);
    }

    result = aplist_fcache_get(WCG(lfcache), filename, &fullpath, &pfvalue TSRMLS_CC);
    if(FAILED(result))
    {
        if(result == FATAL_FCACHE_ORIGINAL_OPEN)
        {
            return FAILURE;
        }

        goto Finished;
    }

    if(pfvalue != NULL)
    {
        result = aplist_fcache_use(WCG(lfcache), fullpath, pfvalue, &file_handle);
        if(FAILED(result))
        {
            aplist_fcache_close(WCG(lfcache), pfvalue);
            goto Finished;
        }

        file_handle->free_filename = 1;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        /* We can fail for big files or if we are not able to find file */
        /* If we fail, let original stream open function do its job */
        dprintimportant("wincache_stream_open_function failed with error %u", result);
        _ASSERT(FALSE);

        if(fullpath != NULL)
        {
            alloc_efree(fullpath);
            fullpath = NULL;
        }

        return original_stream_open_function(filename, file_handle TSRMLS_CC);
    }

    return SUCCESS;
}

zend_op_array * wincache_compile_file(zend_file_handle * file_handle, int type TSRMLS_DC)
{
    int              result   = NONFATAL;
    int              dummy    = -1;
    char *           filename = NULL;
    char *           fullpath = NULL;

    zend_op_array *  oparray  = NULL;
    ocacheval_list * pentry   = NULL;
    ocache_value *   povalue  = NULL;
    unsigned char    cenabled = 0;

    cenabled = WCG(ocenabled);

    /* If ocenabled is not modified in php code and toggle is set, change cenabled */
    if(ini_entries[OCENABLED_INDEX_IN_GLOBALS].modified == 0 && WCG(dooctoggle) == 1)
    {
        /* toggle the current value of cenabled */
        cenabled = !cenabled;
    }

    /* If effective value of wincache.ocenabled is 0, use original_compile_file */
    /* or if file_handle is passed null, use original_compile_file */
    if(!cenabled || file_handle == NULL)
    {
        oparray = original_compile_file(file_handle, type TSRMLS_CC);
        goto Finished;
    }

    dprintverbose("start wincache_compile_file");

    _ASSERT(WCG(locache)          != NULL);
    _ASSERT(WCG(locache)->pocache != NULL);

    /* If filename is not set, use original_compile_file */
    filename = utils_filepath(file_handle);
    if(filename == NULL)
    {
        oparray = original_compile_file(file_handle, type TSRMLS_CC);
        goto Finished;
    }

    /* Use file cache to expand relative paths and also standardize all paths */
    /* Keep last argument as NULL to indicate that we only want fullpath of file */
    /* TBD?? Make sure all caches can be used regardless of enabled setting */
    result = aplist_fcache_get(WCG(lfcache), filename, &fullpath, NULL TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    dprintimportant("compile_file called for %s", fullpath);

    /* Hook so that test file is not added to cache */
    if(strstr(filename, WINCACHE_TEST_FILE) != NULL || isin_ignorelist(WCG(ignorelist), filename))
    {
        dprintimportant("cache is disabled for the file because of ignore list");
        
        oparray = original_compile_file(file_handle, type TSRMLS_CC);
        goto Finished;
    }

    /* Add the file before calling compile_file so that file is */
    /* engine doesn't try to compile file again if it detects so */
    zend_hash_add(&EG(included_files), fullpath, strlen(fullpath) + 1, (void *)&dummy, sizeof(int), NULL);

    result = aplist_ocache_get(WCG(locache), fullpath, file_handle, type, &oparray, &povalue TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(oparray == NULL)
    {
        _ASSERT(povalue != NULL);

        result = aplist_ocache_use(WCG(locache), povalue, &oparray TSRMLS_CC);
        if(FAILED(result))
        {
            aplist_ocache_close(WCG(locache), povalue);
            goto Finished;
        }

        /* If everything went file, add the file handle to open_files */
        /* list for PHP core to call the destructor when done */
        if(file_handle->opened_path == NULL)
        {
           file_handle->opened_path = alloc_estrdup(filename);
        }

        zend_llist_add_element(&CG(open_files), file_handle);

        _ASSERT(SUCCEEDED(result));

        pentry = (ocacheval_list *)alloc_emalloc(sizeof(ocacheval_list));
        if(pentry == NULL)
        {
            aplist_ocache_close(WCG(locache), povalue);
            result = FATAL_OUT_OF_LMEMORY;

            goto Finished;
        }

        pentry->pvalue = povalue;
        pentry->next   = NULL;

        if(WCG(oclisttail) == NULL)
        {
            WCG(oclisthead) = pentry;
            WCG(oclisttail) = pentry;
        }
        else
        {
            WCG(oclisttail)->next = pentry;
            WCG(oclisttail) = pentry;
        }
    }

Finished:

    if(fullpath != NULL)
    {
        alloc_efree(fullpath);
        fullpath = NULL;
    }

    if(FAILED(result))
    {
        if(result != FATAL_OPCOPY_MISSING_PARENT)
        {
            dprintimportant("failure %d in wincache_compile_file", result);
            _ASSERT(FALSE);
        }

        /* If we fail to compile file, let PHP core give a try */
        oparray = original_compile_file(file_handle, type TSRMLS_CC);
    }

    dprintverbose("end wincache_compile_file");

    return oparray;
}

static void errmsglist_dtor(void * pvoid)
{
    ocache_user_message * pmessage = NULL;

    _ASSERT(pvoid != NULL);
    pmessage = (ocache_user_message *)pvoid;

    if(pmessage->filename != NULL)
    {
        alloc_efree(pmessage->filename);
        pmessage->filename = NULL;
    }

    if(pmessage->message != NULL)
    {
        alloc_efree(pmessage->message);
        pmessage->message = NULL;
    }

    return;
}

void wincache_error_cb(int type, const char * error_filename, const uint error_lineno, const char * format, va_list args)
{
    ocache_user_message message = {0};
    char *              buffer  = NULL;

    TSRMLS_FETCH();

    /* First call to wincache_error_cb creates the errmsglist */
    if(WCG(errmsglist) == NULL)
    {
        WCG(errmsglist) = (zend_llist *)alloc_emalloc(sizeof(zend_llist));
        if(WCG(errmsglist) == NULL)
        {
            goto Finished;
        }
        
        zend_llist_init(WCG(errmsglist), sizeof(ocache_user_message), errmsglist_dtor, 0);
    }
    
    if(WCG(errmsglist) != NULL)
    {
        message.type = type;
        message.filename = alloc_estrdup(error_filename);
        if(message.filename == NULL)
        {
            goto Finished;
        }

        message.lineno = error_lineno;
        vspprintf(&buffer, PG(log_errors_max_len), format, args);
        if(buffer == NULL)
        {
            goto Finished;
        }

        message.message = alloc_estrdup(buffer);
        if(message.message == NULL)
        {
            goto Finished;
        }

        /* Add message generated to error message list */
        zend_llist_add_element(WCG(errmsglist), &message);
    }

Finished:

    if(buffer != NULL)
    {
        efree(buffer);
        buffer = NULL;
    }

    original_error_cb(type, error_filename, error_lineno, format, args);
    return;
}

/* Functions */
PHP_FUNCTION(wincache_rplist_fileinfo)
{
    int                  result    = NONFATAL;
    rplist_info *        pcinfo    = NULL;
    rplist_entry_info *  peinfo    = NULL;
    zval *               zfentries = NULL;
    zval *               zfentry   = NULL;
    unsigned int         index     = 1;

    if(WCG(lfcache) == NULL)
    {
        goto Finished;
    }

    result = rplist_getinfo(WCG(lfcache)->prplist, &pcinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);
    add_assoc_long(return_value, "total_file_count", pcinfo->itemcount);

    MAKE_STD_ZVAL(zfentries);
    array_init(zfentries);

    peinfo = pcinfo->entries;
    while(peinfo != NULL)
    {
        MAKE_STD_ZVAL(zfentry);
        array_init(zfentry);

        add_assoc_string(zfentry, "relative_path", peinfo->pathkey, 1);
        add_assoc_string(zfentry, "subkey_data", peinfo->subkey, 1);

        if(peinfo->abspath != NULL)
        {
            add_assoc_string(zfentry, "absolute_path", peinfo->abspath, 1);
        }

        add_index_zval(zfentries, index, zfentry);
        peinfo = peinfo->next;
        index++;
    }

    add_assoc_zval(return_value, "rplist_entries", zfentries);

Finished:

    if(pcinfo != NULL)
    {
        rplist_freeinfo(pcinfo);
        pcinfo = NULL;
    }

    return;
}

PHP_FUNCTION(wincache_rplist_meminfo)
{
    int          result = NONFATAL;
    alloc_info * pinfo  = NULL;

    if(WCG(lfcache) == NULL)
    {
        goto Finished;
    }

    result = alloc_getinfo(WCG(lfcache)->prplist->rpalloc, &pinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);

    add_assoc_long(return_value, "memory_total", pinfo->total_size);
    add_assoc_long(return_value, "memory_free", pinfo->free_size);
    add_assoc_long(return_value, "num_used_blks", pinfo->usedcount);
    add_assoc_long(return_value, "num_free_blks", pinfo->freecount);
    add_assoc_long(return_value, "memory_overhead", pinfo->mem_overhead);

Finished:

    if(pinfo != NULL)
    {
        alloc_freeinfo(pinfo);
        pinfo = NULL;
    }

    return;
}

PHP_FUNCTION(wincache_fcache_fileinfo)
{
    int                  result    = NONFATAL;
    cache_info *         pcinfo    = NULL;
    cache_entry_info *   peinfo    = NULL;
    fcache_entry_info *  pinfo     = NULL;
    zval *               zfentries = NULL;
    zval *               zfentry   = NULL;
    unsigned int         index     = 1;

    if(WCG(lfcache) == NULL)
    {
        goto Finished;
    }

    result = aplist_getinfo(WCG(lfcache), CACHE_TYPE_FILECONTENT, &pcinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);

    add_assoc_long(return_value, "total_cache_uptime", pcinfo->initage);
    add_assoc_long(return_value, "total_file_count", pcinfo->itemcount);
    add_assoc_long(return_value, "total_hit_count", pcinfo->hitcount);
    add_assoc_long(return_value, "total_miss_count", pcinfo->misscount);

    MAKE_STD_ZVAL(zfentries);
    array_init(zfentries);

    peinfo = pcinfo->entries;
    while(peinfo != NULL)
    {
        MAKE_STD_ZVAL(zfentry);
        array_init(zfentry);
        add_assoc_string(zfentry, "file_name", peinfo->filename, 1);
        add_assoc_long(zfentry, "add_time", peinfo->addage);
        add_assoc_long(zfentry, "use_time", peinfo->useage);
        add_assoc_long(zfentry, "last_check", peinfo->lchkage);

        if(peinfo->cdata != NULL)
        {
            pinfo = (fcache_entry_info *)peinfo->cdata;
            add_assoc_long(zfentry, "file_size", pinfo->filesize);
            add_assoc_long(zfentry, "hit_count", pinfo->hitcount);
        }

        add_index_zval(zfentries, index, zfentry);
        peinfo = peinfo->next;
        index++;
    }

    add_assoc_zval(return_value, "file_entries", zfentries);

Finished:

    if(pcinfo != NULL)
    {
        aplist_freeinfo(CACHE_TYPE_FILECONTENT, pcinfo);
        pcinfo = NULL;
    }

    return;
}

PHP_FUNCTION(wincache_fcache_meminfo)
{
    int          result = NONFATAL;
    alloc_info * pinfo  = NULL;

    if(WCG(lfcache) == NULL)
    {
        goto Finished;
    }

    result = alloc_getinfo(WCG(lfcache)->pfcache->palloc, &pinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);

    add_assoc_long(return_value, "memory_total", pinfo->total_size);
    add_assoc_long(return_value, "memory_free", pinfo->free_size);
    add_assoc_long(return_value, "num_used_blks", pinfo->usedcount);
    add_assoc_long(return_value, "num_free_blks", pinfo->freecount);
    add_assoc_long(return_value, "memory_overhead", pinfo->mem_overhead);

Finished:

    if(pinfo != NULL)
    {
        alloc_freeinfo(pinfo);
        pinfo = NULL;
    }

    return;
}

PHP_FUNCTION(wincache_ocache_fileinfo)
{
    int                  result    = NONFATAL;
    cache_info *         pcinfo    = NULL;
    cache_entry_info *   peinfo    = NULL;
    ocache_entry_info *  pinfo     = NULL;
    zval *               zfentries = NULL;
    zval *               zfentry   = NULL;
    unsigned int         index     = 1;

    if(WCG(locache) == NULL)
    {
        goto Finished;
    }

    result = aplist_getinfo(WCG(locache), CACHE_TYPE_BYTECODES, &pcinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);

    add_assoc_long(return_value, "total_cache_uptime", pcinfo->initage);
    add_assoc_long(return_value, "total_file_count", pcinfo->itemcount);
    add_assoc_long(return_value, "total_hit_count", pcinfo->hitcount);
    add_assoc_long(return_value, "total_miss_count", pcinfo->misscount);

    MAKE_STD_ZVAL(zfentries);
    array_init(zfentries);

    peinfo = pcinfo->entries;
    while(peinfo != NULL)
    {
        MAKE_STD_ZVAL(zfentry);
        array_init(zfentry);

        add_assoc_string(zfentry, "file_name", peinfo->filename, 1);
        add_assoc_long(zfentry, "add_time", peinfo->addage);
        add_assoc_long(zfentry, "use_time", peinfo->useage);
        add_assoc_long(zfentry, "last_check", peinfo->lchkage);

        if(peinfo->cdata != NULL)
        {
            pinfo = (ocache_entry_info *)peinfo->cdata;
            add_assoc_long(zfentry, "hit_count", pinfo->hitcount);
            add_assoc_long(zfentry, "function_count", pinfo->fcount);
            add_assoc_long(zfentry, "class_count", pinfo->ccount);
        }

        add_index_zval(zfentries, index, zfentry);
        peinfo = peinfo->next;
        index++;
    }

    add_assoc_zval(return_value, "file_entries", zfentries);

Finished:

    if(pcinfo != NULL)
    {
        aplist_freeinfo(CACHE_TYPE_BYTECODES, pcinfo);
        pcinfo = NULL;
    }

    return;
}

PHP_FUNCTION(wincache_ocache_meminfo)
{
    int          result = NONFATAL;
    alloc_info * pinfo  = NULL;

    if(WCG(locache) == NULL)
    {
        goto Finished;
    }

    result = alloc_getinfo(WCG(locache)->pocache->palloc, &pinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);

    add_assoc_long(return_value, "memory_total", pinfo->total_size);
    add_assoc_long(return_value, "memory_free", pinfo->free_size);
    add_assoc_long(return_value, "num_used_blks", pinfo->usedcount);
    add_assoc_long(return_value, "num_free_blks", pinfo->freecount);
    add_assoc_long(return_value, "memory_overhead", pinfo->mem_overhead);

Finished:

    if(pinfo != NULL)
    {
        alloc_freeinfo(pinfo);
        pinfo = NULL;
    }

    return;
}

PHP_FUNCTION(wincache_refresh_if_changed)
{
    int    result   = NONFATAL;
    zval * filelist = NULL;

    /* If both file cache and opcode cache are not active, return false */
    if(WCG(lfcache) == NULL && WCG(locache) == NULL)
    {
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!", &filelist) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    if(!ZEND_NUM_ARGS())
    {
        /* last check of all the entries need to be changed */
        filelist = NULL;
    }

    result = aplist_force_fccheck(WCG(lfcache), filelist TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(WCG(lfcache) != WCG(locache))
    {
        result = aplist_force_fccheck(WCG(locache), filelist TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in wincache_refresh_if_changed", result);
        _ASSERT(FALSE);

        RETURN_FALSE;
    }

    RETURN_TRUE;
}

#ifdef WINCACHE_TEST
PHP_FUNCTION(wincache_runtests)
{
    dprintverbose("start wincache_runtests");

    lock_runtest();
    filemap_runtest();
    alloc_runtest();
    aplist_runtest();
    rplist_runtest();
    ocache_runtest();
    opcopy_runtest();
    fcache_runtest();
    optimizer_runtest();
    
    dprintverbose("end wincache_runtests");
    return;
}
#endif

#ifdef WINCACHE_TEST
PHP_FUNCTION(wincache_fcache_find)
{
    int                  result    = NONFATAL;
    cache_info *         pcinfo    = NULL;
    cache_entry_info *   peinfo    = NULL;
    char *               filename  = NULL;
    unsigned int         filelen   = 0;
    int                  found     = 0;

    if(WCG(lfcache) == NULL ||
       zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filelen) == FAILURE)
    {
        goto Finished;
    }

    result = aplist_getinfo(WCG(lfcache), CACHE_TYPE_FILECONTENT, &pcinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    peinfo = pcinfo->entries;
    while(peinfo != NULL)
    {
        if (!_stricmp(peinfo->filename, filename))
        {
            found = 1;
            break;
        }

        peinfo = peinfo->next;
    }

Finished:

    if(pcinfo != NULL)
    {
        aplist_freeinfo(CACHE_TYPE_FILECONTENT, pcinfo);
        pcinfo = NULL;
    }

    if(found)
    {
        RETURN_TRUE;
    }

    RETURN_FALSE;    
}
#endif

#ifdef WINCACHE_TEST
PHP_FUNCTION(wincache_ocache_find)
{
    int            result = NONFATAL;
    ocache_value * ovalue = NULL;
    int            found  = 0;
    unsigned int   index  = 0;

    char * filename = NULL, * searchitem = NULL, * type = NULL;
    int filelen = 0, searchlen = 0, typelen = 0;

    if(WCG(locache) == NULL ||
       zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &filename, &filelen, &searchitem, &searchlen, &type, &typelen) == FAILURE)
    {
        goto Finished;
    }

    result = aplist_ocache_get_value(WCG(locache), filename, &ovalue);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(!_stricmp(type, "function"))
    {
        for(index = 0; index < ovalue->fcount; index++)
        {
            if(!_stricmp(ovalue->functions[index].fname, searchitem))
            {
                found = 1;
                break;
            }
        }
    }

    if(!_stricmp(type, "class"))
    {
        for(index = 0; index < ovalue->ccount; index++)
        {
            if(!_stricmp(ovalue->classes[index].cname, searchitem))
            {
                found = 1;
                break;
            }
        }
    }

    aplist_ocache_close(WCG(locache), ovalue);

Finished:

    if(found)
    {
        RETURN_TRUE;
    }
    
    RETURN_FALSE;
}
#endif
