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

#define NAMESALT_LENGTH_MAXIMUM     8
#define OCENABLED_INDEX_IN_GLOBALS  1
#define FCENABLED_INDEX_IN_GLOBALS  0
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
PHP_FUNCTION(wincache_ucache_meminfo);
PHP_FUNCTION(wincache_scache_meminfo);

/* Utility functions exposed by this extension */
PHP_FUNCTION(wincache_refresh_if_changed);
PHP_FUNCTION(wincache_function_exists);
PHP_FUNCTION(wincache_class_exists);

/* ZVAL cache functions matching other caches */
PHP_FUNCTION(wincache_ucache_get);
PHP_FUNCTION(wincache_ucache_set);
PHP_FUNCTION(wincache_ucache_add);
PHP_FUNCTION(wincache_ucache_delete);
PHP_FUNCTION(wincache_ucache_clear);
PHP_FUNCTION(wincache_ucache_exists);
PHP_FUNCTION(wincache_ucache_info);
PHP_FUNCTION(wincache_scache_info);
PHP_FUNCTION(wincache_ucache_inc);
PHP_FUNCTION(wincache_ucache_dec);
PHP_FUNCTION(wincache_ucache_cas);

#ifdef WINCACHE_TEST
PHP_FUNCTION(wincache_ucache_lasterror);
PHP_FUNCTION(wincache_runtests);
PHP_FUNCTION(wincache_fcache_find);
PHP_FUNCTION(wincache_ocache_find);
#endif

ZEND_DECLARE_MODULE_GLOBALS(wincache)

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_rplist_fileinfo, 0, 0, 0)
    ZEND_ARG_INFO(0, summaryonly)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_rplist_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_fcache_fileinfo, 0, 0, 0)
    ZEND_ARG_INFO(0, summaryonly)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_fcache_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ocache_fileinfo, 0, 0, 0)
    ZEND_ARG_INFO(0, summaryonly)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ocache_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_scache_meminfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_refresh_if_changed, 0, 0, 0)
    ZEND_ARG_INFO(0, file_list)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_function_exists, 0, 0, 1)
    ZEND_ARG_INFO(0, function_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_class_exists, 0, 0, 1)
    ZEND_ARG_INFO(0, class_name)
    ZEND_ARG_INFO(0, autoload)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_get, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(1, success)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_set, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()
        
ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_add, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_delete, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_clear, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_exists, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_info, 0, 0, 0)
    ZEND_ARG_INFO(0, summaryonly)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_scache_info, 0, 0, 0)
    ZEND_ARG_INFO(0, summaryonly)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_inc, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, delta)
    ZEND_ARG_INFO(1, success)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_dec, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, delta)
    ZEND_ARG_INFO(1, success)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_cas, 0, 0, 3)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, cvalue)
    ZEND_ARG_INFO(0, nvalue)
ZEND_END_ARG_INFO()

#ifdef WINCACHE_TEST
ZEND_BEGIN_ARG_INFO_EX(arginfo_wincache_ucache_lasterror, 0, 0, 0)
ZEND_END_ARG_INFO()

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
    PHP_FE(wincache_ucache_meminfo, arginfo_wincache_ucache_meminfo)
    PHP_FE(wincache_scache_meminfo, arginfo_wincache_scache_meminfo)
    PHP_FE(wincache_refresh_if_changed, arginfo_wincache_refresh_if_changed)
    PHP_FE(wincache_function_exists, arginfo_wincache_function_exists)
    PHP_FE(wincache_class_exists, arginfo_wincache_class_exists)
    PHP_FE(wincache_ucache_get, arginfo_wincache_ucache_get)
    PHP_FE(wincache_ucache_set, arginfo_wincache_ucache_set)
    PHP_FE(wincache_ucache_add, arginfo_wincache_ucache_add)
    PHP_FE(wincache_ucache_delete, arginfo_wincache_ucache_delete)
    PHP_FE(wincache_ucache_clear, arginfo_wincache_ucache_clear)
    PHP_FE(wincache_ucache_exists, arginfo_wincache_ucache_exists)
    PHP_FE(wincache_ucache_info, arginfo_wincache_ucache_info)
    PHP_FE(wincache_scache_info, arginfo_wincache_scache_info)
    PHP_FE(wincache_ucache_inc, arginfo_wincache_ucache_inc)
    PHP_FE(wincache_ucache_dec, arginfo_wincache_ucache_dec)
    PHP_FE(wincache_ucache_cas, arginfo_wincache_ucache_cas)
#ifdef WINCACHE_TEST
    PHP_FE(wincache_ucache_lasterror, arginfo_wincache_ucache_lasterror)
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
/* index 3 */  STD_PHP_INI_ENTRY("wincache.fcachesize", "24", PHP_INI_SYSTEM, OnUpdateLong, fcachesize, zend_wincache_globals, wincache_globals)
/* index 4 */  STD_PHP_INI_ENTRY("wincache.ocachesize", "96", PHP_INI_SYSTEM, OnUpdateLong, ocachesize, zend_wincache_globals, wincache_globals)
/* index 5 */  STD_PHP_INI_ENTRY("wincache.maxfilesize", "256", PHP_INI_SYSTEM, OnUpdateLong, maxfilesize, zend_wincache_globals, wincache_globals)
/* index 6 */  STD_PHP_INI_ENTRY("wincache.filecount", "4096", PHP_INI_SYSTEM, OnUpdateLong, numfiles, zend_wincache_globals, wincache_globals)
/* index 7 */  STD_PHP_INI_ENTRY("wincache.chkinterval", "30", PHP_INI_SYSTEM, OnUpdateLong, fcchkfreq, zend_wincache_globals, wincache_globals)
/* index 8 */  STD_PHP_INI_ENTRY("wincache.ttlmax", "1200", PHP_INI_SYSTEM, OnUpdateLong, ttlmax, zend_wincache_globals, wincache_globals)
/* index 9 */  STD_PHP_INI_ENTRY("wincache.debuglevel", "0", PHP_INI_SYSTEM, OnUpdateLong, debuglevel, zend_wincache_globals, wincache_globals)
/* index 10 */ STD_PHP_INI_ENTRY("wincache.ignorelist", NULL, PHP_INI_ALL, OnUpdateString, ignorelist, zend_wincache_globals, wincache_globals)
/* index 11 */ STD_PHP_INI_ENTRY("wincache.ocenabledfilter", NULL, PHP_INI_SYSTEM, OnUpdateString, ocefilter, zend_wincache_globals, wincache_globals)
/* index 12 */ STD_PHP_INI_ENTRY("wincache.fcenabledfilter", NULL, PHP_INI_SYSTEM, OnUpdateString, fcefilter, zend_wincache_globals, wincache_globals)
/* index 13 */ STD_PHP_INI_ENTRY("wincache.namesalt", NULL, PHP_INI_SYSTEM, OnUpdateString, namesalt, zend_wincache_globals, wincache_globals)
/* index 14 */ STD_PHP_INI_ENTRY("wincache.localheap", "0", PHP_INI_SYSTEM, OnUpdateBool, localheap, zend_wincache_globals, wincache_globals)
/* index 15 */ STD_PHP_INI_BOOLEAN("wincache.ucenabled", "1", PHP_INI_ALL, OnUpdateBool, ucenabled, zend_wincache_globals, wincache_globals)
/* index 16 */ STD_PHP_INI_ENTRY("wincache.ucachesize", "8", PHP_INI_SYSTEM, OnUpdateLong, ucachesize, zend_wincache_globals, wincache_globals)
/* index 17 */ STD_PHP_INI_ENTRY("wincache.scachesize", "8", PHP_INI_SYSTEM, OnUpdateLong, scachesize, zend_wincache_globals, wincache_globals)
/* index 18 */ STD_PHP_INI_ENTRY("wincache.rerouteini", NULL, PHP_INI_SYSTEM, OnUpdateString, rerouteini, zend_wincache_globals, wincache_globals)
#ifdef WINCACHE_TEST
/* index 19 */ STD_PHP_INI_ENTRY("wincache.olocaltest", "0", PHP_INI_SYSTEM, OnUpdateBool, olocaltest, zend_wincache_globals, wincache_globals)
#endif
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
    WCG(ucenabled)   = 1;    /* User cache enabled by default */
    WCG(enablecli)   = 0;    /* WinCache not enabled by default for CLI */
    WCG(fcachesize)  = 24;   /* File cache size is 24 MB by default */
    WCG(ocachesize)  = 96;   /* Opcode cache size is 96 MB by default */
    WCG(ucachesize)  = 8;    /* User cache size is 8 MB by default */
    WCG(scachesize)  = 8;    /* Session cache size is 8 MB by default */
    WCG(maxfilesize) = 256;  /* Maximum file size to cache is 256 KB */
    WCG(numfiles)    = 4096; /* 4096 hashtable keys */
    WCG(fcchkfreq)   = 30;   /* File change check done every 30 secs */
    WCG(ttlmax)      = 1200; /* File removed if not used for 1200 secs */
    WCG(debuglevel)  = 0;    /* Debug dump not enabled by default */
    WCG(ignorelist)  = NULL; /* List of files to ignore for caching */
    WCG(ocefilter)   = NULL; /* List of sites for which ocenabled is toggled */
    WCG(fcefilter)   = NULL; /* List of sites for which fcenabled is toggled */
    WCG(namesalt)    = NULL; /* Salt to use in names used by wincache */
    WCG(rerouteini)  = NULL; /* Ini file containing function and class reroutes */
    WCG(localheap)   = 0;    /* Local heap is disabled by default */

    WCG(lasterror)   = 0;    /* GetLastError() value set by wincache */
    WCG(uclasterror) = 0;    /* Last error returned by user cache */
    WCG(lfcache)     = NULL; /* Aplist to use for file/rpath cache */
    WCG(locache)     = NULL; /* Aplist to use for opcode cache */
    WCG(zvucache)    = NULL; /* zvcache_context for user zvals */
    WCG(detours)     = NULL; /* detours_context containing reroutes */
    WCG(zvscache)    = NULL; /* zvcache_context for session data */
    WCG(issame)      = 1;    /* Indicates if same aplist is used */
    WCG(zvcopied)    = NULL; /* HashTable which helps with refcounting */
    WCG(oclisthead)  = NULL; /* Head of in-use ocache values list */
    WCG(oclisttail)  = NULL; /* Tail of in-use ocache values list */
    WCG(parentpid)   = 0;    /* Parent process identifier to use */
    WCG(fmapgdata)   = NULL; /* Global filemap information data */
    WCG(errmsglist)  = NULL; /* Error messages list generated by PHP */
    WCG(dooctoggle)  = 0;    /* If set to 1, toggle value of ocenabled */
    WCG(dofctoggle)  = 0;    /* If set to 1, toggle value of fcenabled */

#ifdef WINCACHE_TEST
    WCG(olocaltest)  = 0;    /* Local opcode test disabled by default */
#endif

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
    int                result    = NONFATAL;
    aplist_context *   plcache1  = NULL;
    aplist_context *   plcache2  = NULL;
    zvcache_context *  pzcache   = NULL;
    int                resnumber = -1;
    zend_extension     extension = {0};
    detours_context *  pdetours  = NULL;
    detours_info *     pdinfo    = NULL;

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
    WCG(ucachesize)  = (WCG(ucachesize)  < ZCACHE_SIZE_MINIMUM) ? ZCACHE_SIZE_MINIMUM : WCG(ucachesize);
    WCG(ucachesize)  = (WCG(ucachesize)  > ZCACHE_SIZE_MAXIMUM) ? ZCACHE_SIZE_MAXIMUM : WCG(ucachesize);
    WCG(scachesize)  = (WCG(scachesize)  < ZCACHE_SIZE_MINIMUM) ? ZCACHE_SIZE_MINIMUM : WCG(scachesize);
    WCG(scachesize)  = (WCG(scachesize)  > ZCACHE_SIZE_MAXIMUM) ? ZCACHE_SIZE_MAXIMUM : WCG(scachesize);
    WCG(maxfilesize) = (WCG(maxfilesize) < FILE_SIZE_MINIMUM)   ? FILE_SIZE_MINIMUM   : WCG(maxfilesize);
    WCG(maxfilesize) = (WCG(maxfilesize) > FILE_SIZE_MAXIMUM)   ? FILE_SIZE_MAXIMUM   : WCG(maxfilesize);

    /* ttlmax can be set to 0 which means scavenger is completely disabled */
    if(WCG(ttlmax) != 0)
    {
        WCG(ttlmax)    = (WCG(ttlmax)      < TTL_VALUE_MINIMUM)   ? TTL_VALUE_MINIMUM   : WCG(ttlmax);
        WCG(ttlmax)    = (WCG(ttlmax)      > TTL_VALUE_MAXIMUM)   ? TTL_VALUE_MAXIMUM   : WCG(ttlmax);
    }

    /* fcchkfreq can be set to 0 which will mean check is completely disabled */
    if(WCG(fcchkfreq) != 0)
    {
        WCG(fcchkfreq) = (WCG(fcchkfreq)   < FCHECK_FREQ_MINIMUM) ? FCHECK_FREQ_MINIMUM : WCG(fcchkfreq);
        WCG(fcchkfreq) = (WCG(fcchkfreq)   > FCHECK_FREQ_MAXIMUM) ? FCHECK_FREQ_MAXIMUM : WCG(fcchkfreq);
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

    /* Enforce opcode cache size to be at least 3 times the file cache size */
    if(WCG(ocachesize) < 3 * WCG(fcachesize))
    {
        WCG(ocachesize) = 3 * WCG(fcachesize);
    }

    /* Even if enabled is set to false, create cache and set */
    /* the hook because scripts can selectively enable it */

    /* Call filemap global initialized. Terminate in MSHUTDOWN */
    result = filemap_global_initialize(TSRMLS_C);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(WCG(rerouteini) != NULL)
    {
        result = detours_create(&pdetours);
        if(FAILED(result))
        {
            goto Finished;
        }

        result = detours_initialize(pdetours, WCG(rerouteini));
        if(FAILED(result))
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "failure initializing function reroute as per wincache.rerouteini");
            goto Finished;
        }

        WCG(detours) = pdetours;
    }

    /* Create filelist cache */
    result = aplist_create(&plcache1);
    if(FAILED(result))
    {
        goto Finished;
    }

    result = aplist_initialize(plcache1, APLIST_TYPE_GLOBAL, WCG(numfiles), WCG(fcchkfreq), WCG(ttlmax) TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Initialize file cache */
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

    /* Initialize opcode cache */
    resnumber = zend_get_resource_handle(&extension);
    if(resnumber == -1)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

#ifdef WINCACHE_TEST
    /* If olocaltest is set, force a local opcode cache sometimes */
    if(WCG(olocaltest) != 0 && plcache1->apheader->mapcount % 2 == 0)
    {
        result = WARNING_FILEMAP_MAPVIEW;
    }
    else
    {
        result = aplist_ocache_initialize(plcache1, resnumber, WCG(ocachesize) TSRMLS_CC);
    }
#else
    result = aplist_ocache_initialize(plcache1, resnumber, WCG(ocachesize) TSRMLS_CC);
#endif

    if(FAILED(result))
    {
        if(result != WARNING_FILEMAP_MAPVIEW)
        {
            goto Finished;
        }

        /* Couldn't map at same address, create a local ocache */
        result = aplist_create(&plcache2);
        if(FAILED(result))
        {
            goto Finished;
        }

        result = aplist_initialize(plcache2, APLIST_TYPE_OPCODE_LOCAL, WCG(numfiles), WCG(fcchkfreq), WCG(ttlmax) TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        result = aplist_ocache_initialize(plcache2, resnumber, WCG(ocachesize) TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        /* Disable scavenger and set polocal in global aplist */
        aplist_setsc_olocal(plcache1, plcache2);

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

    /* Create user cache*/
    result = zvcache_create(&pzcache);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* issession = 0, islocal = 0, zvcount = 256, shmfilepath = NULL */
    result = zvcache_initialize(pzcache, 0, 0, 256, WCG(ucachesize), NULL TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Create hashtable zvcopied */
    WCG(zvcopied) = (HashTable *)alloc_pemalloc(sizeof(HashTable));
    if(WCG(zvcopied) == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    WCG(zvucache) = pzcache;

    /* Register wincache session handler */
    php_session_register_module(ps_wincache_ptr);

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in php_minit", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pdetours != NULL)
        {
            detours_terminate(pdetours);
            detours_destroy(pdetours);

            pdetours = NULL;
            WCG(detours) = NULL;
        }

        if(plcache1 != NULL)
        {
            aplist_terminate(plcache1);
            aplist_destroy(plcache1);

            plcache1 = NULL;

            WCG(lfcache) = NULL;
            WCG(locache) = NULL;
        }

        if(plcache2 != NULL)
        {
            aplist_terminate(plcache2);
            aplist_destroy(plcache2);

            plcache2 = NULL;
            WCG(locache) = NULL;
        }

        if(WCG(zvcopied) != NULL)
        {
            alloc_pefree(WCG(zvcopied));
            WCG(zvcopied) = NULL;
        }

        if(pzcache != NULL)
        {
            zvcache_terminate(pzcache);
            zvcache_destroy(pzcache);

            pzcache = NULL;
            WCG(zvucache) = NULL;
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
    
    if(WCG(detours) != NULL)
    {
        detours_terminate(WCG(detours));
        detours_destroy(WCG(detours));

        WCG(detours) = NULL;
    }

    if(WCG(zvcopied) != NULL)
    {
        alloc_pefree(WCG(zvcopied));
        WCG(zvcopied) = NULL;
    }

    if(WCG(lfcache) != NULL)
    {
        aplist_terminate(WCG(lfcache));
        aplist_destroy(WCG(lfcache));

        WCG(lfcache) = NULL;
    }

    if(WCG(locache) != NULL)
    {
        if(!WCG(issame))
        {
            aplist_terminate(WCG(locache));
            aplist_destroy(WCG(locache));
        }

        WCG(locache) = NULL;
    }

    if(WCG(zvucache) != NULL)
    {
        zvcache_terminate(WCG(zvucache));
        zvcache_destroy(WCG(zvucache));

        WCG(zvucache) = NULL;
    }

    if(WCG(zvscache) != NULL)
    {
        zvcache_terminate(WCG(zvscache));
        zvcache_destroy(WCG(zvscache));

        WCG(zvscache) = NULL;
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

    if ((WCG(ocefilter) != NULL) || (WCG(fcefilter) != NULL))
    {
        /* Read site id from list of variables */
        zend_is_auto_global("_SERVER", sizeof("_SERVER") - 1 TSRMLS_CC);

        if (!PG(http_globals)[TRACK_VARS_SERVER] ||
            zend_hash_find(PG(http_globals)[TRACK_VARS_SERVER]->value.ht, "INSTANCE_ID", sizeof("INSTANCE_ID"), (void **) &siteid) == FAILURE)
        {
            goto Finished;
        }
    }

    if(WCG(ocefilter) != NULL && isin_cseplist(WCG(ocefilter), Z_STRVAL_PP(siteid)))
    {
        WCG(dooctoggle) = 1;
    }

    if(WCG(fcefilter) != NULL && isin_cseplist(WCG(fcefilter), Z_STRVAL_PP(siteid)))
    {
        WCG(dofctoggle) = 1;
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
    WCG(dofctoggle) = 0;

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
    int           result       = NONFATAL;
    char *        resolve_path = NULL;
    unsigned char cenabled     = 0;

#ifdef PHP_VERSION_52
    dprintimportant("wincache_resolve_path shouldn't get called for 5.2.X");
    _ASSERT(FALSE);
#endif

    cenabled = WCG(fcenabled);

    /* If fcenabled is not modified in php code and toggle is set, change cenabled */
    if(ini_entries[FCENABLED_INDEX_IN_GLOBALS].modified == 0 && WCG(dofctoggle) == 1)
    {
        /* toggle the current value of cenabled */
        cenabled = !cenabled;
    }

    /* If wincache.fcenabled is set to 0 but some how */
    /* this method is called, use original_resolve_path */
    if(!cenabled || filename == NULL)
    {
        return original_resolve_path(filename, filename_len TSRMLS_CC);
    }
    
    dprintimportant("zend_resolve_path called for %s", filename);

    /* Hook so that test file is not added to cache */
    if(isin_ignorelist(WINCACHE_TEST_FILE, filename) || isin_ignorelist(WCG(ignorelist), filename))
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
        _ASSERT(result > WARNING_COMMON_BASE);

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
    unsigned char  cenabled = 0;

    cenabled = WCG(fcenabled);

    /* If fcenabled is not modified in php code and toggle is set, change cenabled */
    if(ini_entries[FCENABLED_INDEX_IN_GLOBALS].modified == 0 && WCG(dofctoggle) == 1)
    {
        /* toggle the current value of cenabled */
        cenabled = !cenabled;
    }

    /* If wincache.fcenabled is set to 0 but some how */
    /* this method is called, use original_stream_open_function */
    if(!cenabled || filename == NULL)
    {
        return original_stream_open_function(filename, file_handle TSRMLS_CC);
    }

    dprintimportant("zend_stream_open_function called for %s", filename);

    /* Hook so that test file is not added to cache */
    if(isin_ignorelist(WINCACHE_TEST_FILE, filename) || isin_ignorelist(WCG(ignorelist), filename))
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
        _ASSERT(result > WARNING_COMMON_BASE);

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

    dprintverbose("start wincache_compile_file");

    _ASSERT(WCG(locache)          != NULL);
    _ASSERT(WCG(locache)->pocache != NULL);

    cenabled = WCG(ocenabled);

    /* If ocenabled is not modified in php code and toggle is set, change cenabled */
    if(ini_entries[OCENABLED_INDEX_IN_GLOBALS].modified == 0 && WCG(dooctoggle) == 1)
    {
        /* toggle the current value of cenabled */
        cenabled = !cenabled;
    }

    /* If effective value of wincache.ocenabled is 0,  or if file_handle is passed null, */
    /* or if filename is test_file or if the file is in ignorelist, use original_compile_file */
    if(cenabled && file_handle != NULL)
    {
        filename = utils_filepath(file_handle);
    }

    /* Nothing to cleanup. So original_compile triggering bailout is fine */
    if(filename == NULL || isin_ignorelist(WINCACHE_TEST_FILE, filename) || isin_ignorelist(WCG(ignorelist), filename))
    {
        oparray = original_compile_file(file_handle, type TSRMLS_CC);
        goto Finished;
    }

    /* Use file cache to expand relative paths and also standardize all paths */
    /* Keep last argument as NULL to indicate that we only want fullpath of file */
    /* Make sure all caches can be used regardless of enabled setting */
    result = aplist_fcache_get(WCG(lfcache), filename, &fullpath, NULL TSRMLS_CC);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Add the file before calling compile_file so that engine doesn't */
    /* try to compile file again if it detects it in included_files list */
    if(file_handle->opened_path != NULL)
    {
        dprintimportant("wincache_compile_file called for %s", file_handle->opened_path);
        zend_hash_add(&EG(included_files), file_handle->opened_path, strlen(file_handle->opened_path) + 1, (void *)&dummy, sizeof(int), NULL);
    }
    else
    {
        dprintimportant("wincache_compile_file called for %s", fullpath);
        zend_hash_add(&EG(included_files), fullpath, strlen(fullpath) + 1, (void *)&dummy, sizeof(int), NULL);

        /* Set opened_path to fullpath */
        file_handle->opened_path = alloc_estrdup(fullpath);
    }

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

    if(FAILED(result) && result != FATAL_ZEND_BAILOUT)
    {
        dprintimportant("failure %d in wincache_compile_file", result);
        _ASSERT(result > WARNING_COMMON_BASE);

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
    int                  result      = NONFATAL;
    rplist_info *        pcinfo      = NULL;
    rplist_entry_info *  peinfo      = NULL;
    zval *               zfentries   = NULL;
    zval *               zfentry     = NULL;
    unsigned int         index       = 1;
    zend_bool            summaryonly = 0;

    if(WCG(lfcache) == NULL)
    {
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &summaryonly) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    result = rplist_getinfo(WCG(lfcache)->prplist, summaryonly, &pcinfo);
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

        add_assoc_string(zfentry, "resolve_path", peinfo->pathkey, 1);
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
    int                  result      = NONFATAL;
    cache_info *         pcinfo      = NULL;
    cache_entry_info *   peinfo      = NULL;
    fcache_entry_info *  pinfo       = NULL;
    zval *               zfentries   = NULL;
    zval *               zfentry     = NULL;
    unsigned int         index       = 1;
    zend_bool            summaryonly = 0;

    if(WCG(lfcache) == NULL)
    {
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &summaryonly) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    result = aplist_getinfo(WCG(lfcache), CACHE_TYPE_FILECONTENT, summaryonly, &pcinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);

    add_assoc_long(return_value, "total_cache_uptime", pcinfo->initage);
    add_assoc_bool(return_value, "is_local_cache", pcinfo->islocal);
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
    int                  result      = NONFATAL;
    cache_info *         pcinfo      = NULL;
    cache_entry_info *   peinfo      = NULL;
    ocache_entry_info *  pinfo       = NULL;
    zval *               zfentries   = NULL;
    zval *               zfentry     = NULL;
    unsigned int         index       = 1;
    zend_bool            summaryonly = 0;

    if(WCG(locache) == NULL)
    {
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &summaryonly) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    result = aplist_getinfo(WCG(locache), CACHE_TYPE_BYTECODES, summaryonly, &pcinfo);
    if(FAILED(result))
    {
        goto Finished;
    }

    array_init(return_value);

    add_assoc_long(return_value, "total_cache_uptime", pcinfo->initage);
    add_assoc_bool(return_value, "is_local_cache", pcinfo->islocal);
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

PHP_FUNCTION(wincache_ucache_meminfo)
{
    int          result = NONFATAL;
    alloc_info * pinfo  = NULL;

    if(WCG(zvucache) == NULL)
    {
        goto Finished;
    }

    result = alloc_getinfo(WCG(zvucache)->zvalloc, &pinfo);
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

PHP_FUNCTION(wincache_scache_meminfo)
{
    int           result = NONFATAL;
    alloc_info *  pinfo  = NULL;

    if(WCG(zvscache) != NULL)
    {
        result = alloc_getinfo(WCG(zvscache)->zvalloc, &pinfo);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    array_init(return_value);

    /* If cache is not initialized, set everything to 0 */
    if(pinfo == NULL)
    {
        add_assoc_long(return_value, "memory_total", 0);
        add_assoc_long(return_value, "memory_free", 0);
        add_assoc_long(return_value, "num_used_blks", 0);
        add_assoc_long(return_value, "num_free_blks", 0);
        add_assoc_long(return_value, "memory_overhead", 0);
    }
    else
    {
        add_assoc_long(return_value, "memory_total", pinfo->total_size);
        add_assoc_long(return_value, "memory_free", pinfo->free_size);
        add_assoc_long(return_value, "num_used_blks", pinfo->usedcount);
        add_assoc_long(return_value, "num_free_blks", pinfo->freecount);
        add_assoc_long(return_value, "memory_overhead", pinfo->mem_overhead);
    }

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
        result = FATAL_UNEXPECTED_FCALL;
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

    /* No need to call on locache as lfcache will trigger deletion */
    /* from locache which inturn will populate the ocache entry again */

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in wincache_refresh_if_changed", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }

    RETURN_TRUE;
}

PHP_FUNCTION(wincache_function_exists)
{
    char *             oname     = NULL;
    int                oname_len = 0;
    char *             name      = NULL;
    int                name_len  = 0;
    zend_function *    func      = NULL;
    char *             lcname    = NULL;
    unsigned int       retval    = 0;
    unsigned int *     pdata     = NULL;
    static HashTable * fe_table  = NULL;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &oname, &oname_len) == FAILURE)
    {
        return;
    }

    /* Initialize static hashtable if not already initialized */
    if(fe_table == NULL)
    {
        fe_table = (HashTable *)alloc_pemalloc(sizeof(HashTable));
        if(fe_table == NULL)
        {
            return;
        }

        zend_hash_init(fe_table, 0, NULL, NULL, 1);
    }

    /* Try to find the function name in local hashtable first */
    if(zend_hash_find(fe_table, oname, oname_len + 1, (void **)&pdata) == SUCCESS)
    {
        RETURN_BOOL(*pdata);
    }

    /* If not found, do what original function_exists does */
    lcname = zend_str_tolower_dup(oname, oname_len);
    name = lcname;
    name_len = oname_len;

    /* Ignore leading "\" */
    if (lcname[0] == '\\')
    {
        name = &lcname[1];
        name_len--;
    }

    retval = (zend_hash_find(EG(function_table), name, name_len+1, (void **)&func) == SUCCESS);
    efree(lcname);

    if(retval && func->type == ZEND_INTERNAL_FUNCTION && func->internal_function.handler == zif_display_disabled_function)
    {
        retval = 0;
    }

    /* Keep this entry in hashtable with original name */
    zend_hash_add(fe_table, oname, oname_len + 1, &retval, sizeof(void *), NULL);

    RETURN_BOOL(retval);
}

PHP_FUNCTION(wincache_class_exists)
{
    char *              class_name      = NULL;
    int                 class_name_len  = 0;
    char *              lc_name         = NULL;
    zend_class_entry ** ce              = NULL;
    int                 found           = 0;
    zend_bool           autoload        = 1;
    unsigned int        retval          = 0;
    unsigned int *      pdata           = NULL;
    static HashTable *  ce_table        = NULL;
    ALLOCA_FLAG(use_heap)

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &class_name, &class_name_len, &autoload) == FAILURE)
    {
        return;
    }

    /* Initialize static hashtable if not already initialized */
    if(ce_table == NULL)
    {
        ce_table = (HashTable *)alloc_pemalloc(sizeof(HashTable));
        if(ce_table == NULL)
        {
            return;
        }

        zend_hash_init(ce_table, 0, NULL, NULL, 1);
    }

    /* Try to find the function name in local hashtable first */
    if(zend_hash_find(ce_table, class_name, class_name_len + 1, (void **)&pdata) == SUCCESS)
    {
        RETURN_BOOL(*pdata);
    }

    if(!autoload)
    {
        char * name = NULL;
        int    len  = 0;

#ifndef PHP_VERSION_52
        lc_name = do_alloca(class_name_len + 1, use_heap);
#else
        lc_name = do_alloca_with_limit(class_name_len + 1, use_heap);
#endif
        zend_str_tolower_copy(lc_name, class_name, class_name_len);

        /* Ignore leading "\" */
        name = lc_name;
        len = class_name_len;
        if (lc_name[0] == '\\')
        {
            name = &lc_name[1];
            len--;
        }
    
        found = zend_hash_find(EG(class_table), name, len + 1, (void **)&ce);
#ifndef PHP_VERSION_52
        free_alloca(lc_name, use_heap);
#else
        free_alloca_with_limit(lc_name, use_heap);
#endif
        retval = (found == SUCCESS && !((*ce)->ce_flags & ZEND_ACC_INTERFACE));
        goto Finished;
    }

    if(zend_lookup_class(class_name, class_name_len, &ce TSRMLS_CC) == SUCCESS)
    {
        retval = (((*ce)->ce_flags & ZEND_ACC_INTERFACE) == 0);
    }
    else
    {
       retval = 0;
    }

Finished:

    /* Keep this entry in hashtable with original name */
    zend_hash_add(ce_table, class_name, class_name_len + 1, &retval, sizeof(void *), NULL);

    RETURN_BOOL(retval);
}

PHP_FUNCTION(wincache_ucache_get)
{
    int          result    = NONFATAL;
    zval *       pzkey     = NULL;
    zval *       success   = NULL;
    HashTable *  htable    = NULL;
    zval **      hentry    = NULL;
    HashPosition hposition;
    zval *       nentry    = NULL;
    char *       key       = NULL;
    unsigned int keylen    = 0;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &pzkey, &success) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    if(success != NULL)
    {
        ZVAL_BOOL(success, 0);
    }

    /* Convert zval to string zval */
    if(Z_TYPE_P(pzkey) != IS_STRING && Z_TYPE_P(pzkey) != IS_ARRAY)
    {
        convert_to_string(pzkey);
    }

    if(Z_TYPE_P(pzkey) == IS_STRING)
    {
        result = zvcache_get(WCG(zvucache), Z_STRVAL_P(pzkey), &return_value TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }
    }
    else if(Z_TYPE_P(pzkey) == IS_ARRAY)
    {
        array_init(return_value);
        htable = Z_ARRVAL_P(pzkey);
        zend_hash_internal_pointer_reset_ex(htable, &hposition);
        while(zend_hash_get_current_data_ex(htable, (void **)&hentry, &hposition) == SUCCESS)
        {
            if(Z_TYPE_PP(hentry) != IS_STRING && Z_TYPE_PP(hentry) != IS_LONG)
            {
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "key array elements can only be string or long");

                result = WARNING_ZVCACHE_ARGUMENT;
                goto Finished;
            }

            if(Z_TYPE_PP(hentry) == IS_LONG)
            {
                spprintf(&key, 0, "%ld", Z_LVAL_PP(hentry));
                keylen = strlen(key);
            }
            else
            {
                _ASSERT(Z_TYPE_PP(hentry) == IS_STRING);

                key = Z_STRVAL_PP(hentry);
                keylen = Z_STRLEN_PP(hentry);
            }

            MAKE_STD_ZVAL(nentry);
            result = zvcache_get(WCG(zvucache), key, &nentry TSRMLS_CC);

            /* Ignore failures and try getting values of other keys */
            if(SUCCEEDED(result))
            {
                zend_hash_add(Z_ARRVAL_P(return_value), key, keylen + 1, &nentry, sizeof(zval *), NULL);
            }

            if(Z_TYPE_PP(hentry) == IS_LONG && key != NULL)
            {
                efree(key);
                key = NULL;
            }

            result = NONFATAL;
            nentry = NULL;
            key    = NULL;
            keylen = 0;
            zend_hash_move_forward_ex(htable, &hposition);
        }
    }
    else
    {
        _ASSERT(FALSE);
    }

    if(success != NULL)
    {
        ZVAL_BOOL(success, 1);
    }

Finished:

    if(nentry != NULL)
    {
        FREE_ZVAL(nentry);
        nentry = NULL;
    }

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_get", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }
}

PHP_FUNCTION(wincache_ucache_set)
{
    int           result   = NONFATAL;
    zval *        pzkey    = NULL;
    zval *        pzval    = NULL;
    int           ttl      = 0;
    HashTable *   htable   = NULL;
    HashPosition  hposition;
    zval **       hentry   = NULL;
    char *        key      = NULL;
    unsigned int  keylen   = 0;
    unsigned int  longkey  = 0;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zl", &pzkey, &pzval, &ttl) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    /* Negative ttl and resource values are not allowed */
    if(ttl < 0)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "ttl cannot be less than 0");

        result = WARNING_ZVCACHE_ARGUMENT;
        goto Finished;
    }

    if(pzval && Z_TYPE_P(pzval) == IS_RESOURCE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "value cannot be a resource");

        result = WARNING_ZVCACHE_RESCOPYIN;
        goto Finished;
    }

    if(Z_TYPE_P(pzkey) != IS_STRING && Z_TYPE_P(pzkey) != IS_ARRAY)
    {
        convert_to_string(pzkey);
    }

    if(Z_TYPE_P(pzkey) == IS_STRING)
    {
        /* Blank string as key is not allowed */
        if(Z_STRLEN_P(pzkey) == 0 || *(Z_STRVAL_P(pzkey)) == '\0')
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "key cannot be blank string");

            result = WARNING_ZVCACHE_ARGUMENT;
            goto Finished;
        }

        /* When first argument is string, value is required */
        if(pzval == NULL || Z_STRLEN_P(pzkey) > 4096)
        {
            result = WARNING_ZVCACHE_ARGUMENT;
            goto Finished;
        }

        /* isadd = 0 */
        result = zvcache_set(WCG(zvucache), Z_STRVAL_P(pzkey), pzval, ttl, 0 TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        ZVAL_BOOL(return_value, 1);
    }
    else if(Z_TYPE_P(pzkey) == IS_ARRAY)
    {
        array_init(return_value);
        htable = Z_ARRVAL_P(pzkey);
        zend_hash_internal_pointer_reset_ex(htable, &hposition);
        while(zend_hash_get_current_data_ex(htable, (void **)&hentry, &hposition) == SUCCESS)
        {
            zend_hash_get_current_key_ex(htable, &key, &keylen, &longkey, 0, &hposition);

            /* We are taking care of long keys properly */
            if(!key && longkey != 0)
            {
                /* Convert longkey to string and use that instead */
                spprintf(&key, 0, "%ld", longkey);
                keylen = strlen(key);
            }

            if(key && *key != '\0')
            {
                if(keylen > 4096 || Z_TYPE_PP(hentry) == IS_RESOURCE)
                {
                    result = WARNING_ZVCACHE_ARGUMENT;
                }
                else
                {
                    /* isadd = 0 */
                    result = zvcache_set(WCG(zvucache), key, *hentry, ttl, 0 TSRMLS_CC);
                }

                if(FAILED(result))
                {
                    add_assoc_long_ex(return_value, key, keylen, -1);
                }

                if(longkey)
                {
                    efree(key);
                    key = NULL;
                }
            }
            else
            {
                add_index_long(return_value, longkey, -1);
            }

            result  = NONFATAL;
            key     = NULL;
            keylen  = 0;
            longkey = 0;
            zend_hash_move_forward_ex(htable, &hposition);
        }
    }
    else
    {
        _ASSERT(FALSE);
    }

Finished:

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_set", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }
}

PHP_FUNCTION(wincache_ucache_add)
{
    int           result   = NONFATAL;
    zval *        pzkey    = NULL;
    zval *        pzval    = NULL;
    int           ttl      = 0;
    HashTable *   htable   = NULL;
    HashPosition  hposition;
    zval **       hentry   = NULL;
    char *        key      = NULL;
    unsigned int  keylen   = 0;
    unsigned int  longkey  = 0;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zl", &pzkey, &pzval, &ttl) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    /* Negative ttl and resource values are not allowed */
    if(ttl < 0)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "ttl cannot be less than 0");

        result = WARNING_ZVCACHE_ARGUMENT;
        goto Finished;
    }

    if(pzval && Z_TYPE_P(pzval) == IS_RESOURCE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "value cannot be a resource");

        result = WARNING_ZVCACHE_RESCOPYIN;
        goto Finished;
    }

    if(Z_TYPE_P(pzkey) != IS_STRING && Z_TYPE_P(pzkey) != IS_ARRAY)
    {
        convert_to_string(pzkey);
    }

    if(Z_TYPE_P(pzkey) == IS_STRING)
    {
        /* Blank string as key is not allowed */
        if(Z_STRLEN_P(pzkey) == 0 || *(Z_STRVAL_P(pzkey)) == '\0')
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "key cannot be blank string");

            result = WARNING_ZVCACHE_ARGUMENT;
            goto Finished;
        }

        /* When first argument is string, value is required */
        if(pzval == NULL || Z_STRLEN_P(pzkey) > 4096)
        {
            result = WARNING_ZVCACHE_ARGUMENT;
            goto Finished;
        }

        /* isadd = 1 */
        result = zvcache_set(WCG(zvucache), Z_STRVAL_P(pzkey), pzval, ttl, 1 TSRMLS_CC);
        if(FAILED(result))
        {
            goto Finished;
        }

        ZVAL_BOOL(return_value, 1);
    }
    else if(Z_TYPE_P(pzkey) == IS_ARRAY)
    {
        array_init(return_value);
        htable = Z_ARRVAL_P(pzkey);
        zend_hash_internal_pointer_reset_ex(htable, &hposition);
        while(zend_hash_get_current_data_ex(htable, (void **)&hentry, &hposition) == SUCCESS)
        {
            zend_hash_get_current_key_ex(htable, &key, &keylen, &longkey, 0, &hposition);

            /* We are taking care of long keys properly */
            if(!key && longkey != 0)
            {
                /* Convert longkey to string and use that instead */
                spprintf(&key, 0, "%ld", longkey);
                keylen = strlen(key);
            }

            if(key && *key != '\0')
            {
                if(keylen > 4096 || Z_TYPE_PP(hentry) == IS_RESOURCE)
                {
                    result = WARNING_ZVCACHE_ARGUMENT;
                }
                else
                {
                    /* isadd = 1 */
                    result = zvcache_set(WCG(zvucache), key, *hentry, ttl, 1 TSRMLS_CC);
                }

                if(FAILED(result))
                {
                    add_assoc_long_ex(return_value, key, keylen, -1);
                }

                if(longkey)
                {
                    efree(key);
                    key = NULL;
                }
            }
            else
            {
                add_index_long(return_value, longkey, -1);
            }

            result  = NONFATAL;
            key     = NULL;
            keylen  = 0;
            longkey = 0;
            zend_hash_move_forward_ex(htable, &hposition);
        }
    }
    else
    {
        _ASSERT(FALSE);
    }

Finished:

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_add", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(result == WARNING_ZVCACHE_EXISTS)
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "function called with a key which already exists");
        }

        RETURN_FALSE;
    }
}

PHP_FUNCTION(wincache_ucache_delete)
{
    int           result   = NONFATAL;
    zval *        pzkey    = NULL;
    HashTable *   htable   = NULL;
    HashPosition  hposition;
    zval **       hentry   = NULL;
    char *        key      = NULL;
    unsigned int  keylen   = 0;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &pzkey) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    if(Z_TYPE_P(pzkey) != IS_STRING && Z_TYPE_P(pzkey) != IS_ARRAY)
    {
        convert_to_string(pzkey);
    }

    if(Z_TYPE_P(pzkey) == IS_STRING)
    {
        result = zvcache_delete(WCG(zvucache), Z_STRVAL_P(pzkey));
        if(FAILED(result))
        {
            goto Finished;
        }

        ZVAL_BOOL(return_value, 1);
    }
    else if(Z_TYPE_P(pzkey) == IS_ARRAY)
    {
        array_init(return_value);
        htable = Z_ARRVAL_P(pzkey);
        zend_hash_internal_pointer_reset_ex(htable, &hposition);
        while(zend_hash_get_current_data_ex(htable, (void **)&hentry, &hposition) == SUCCESS)
        {
            if(Z_TYPE_PP(hentry) != IS_STRING && Z_TYPE_PP(hentry) != IS_LONG)
            {
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "key array elements can only be string or long");

                result = WARNING_ZVCACHE_ARGUMENT;
                goto Finished;
            }

            if(Z_TYPE_PP(hentry) == IS_LONG)
            {
                spprintf(&key, 0, "%ld", Z_LVAL_PP(hentry));
                keylen = strlen(key);
            }
            else
            {
                _ASSERT(Z_TYPE_PP(hentry) == IS_STRING);

                key = Z_STRVAL_PP(hentry);
                keylen = Z_STRLEN_PP(hentry);
            }

            result = zvcache_delete(WCG(zvucache), key);
            if(SUCCEEDED(result))
            {
                add_next_index_zval(return_value, *hentry);
#ifndef PHP_VERSION_52
                (*hentry)->refcount__gc++;
#else
                (*hentry)->refcount++;
#endif
            }

            if(Z_TYPE_PP(hentry) == IS_LONG && key != NULL)
            {
                efree(key);
                key = NULL;
            }

            key    = NULL;
            keylen = 0;
            zend_hash_move_forward_ex(htable, &hposition);
        }
    }
    else
    {
        _ASSERT(FALSE);
    }

Finished:

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_delete", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }
}

PHP_FUNCTION(wincache_ucache_clear)
{
    int result = NONFATAL;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(ZEND_NUM_ARGS())
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    result = zvcache_clear(WCG(zvucache));
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_clear", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }

    RETURN_TRUE;
}

PHP_FUNCTION(wincache_ucache_exists)
{
    int           result = NONFATAL;
    char *        key    = NULL;
    unsigned int  keylen = 0;
    unsigned char exists = 0;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &keylen) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    result = zvcache_exists(WCG(zvucache), key, &exists);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_exists", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }

    if(!exists)
    {
        RETURN_FALSE;
    }

    RETURN_TRUE;
}

PHP_FUNCTION(wincache_ucache_info)
{
    int                  result      = NONFATAL;
    zend_llist *         plist       = NULL;
    zvcache_info_entry * peinfo      = NULL;
    zval *               zfentries   = NULL;
    zval *               zfentry     = NULL;
    unsigned int         index       = 1;
    char *               valuetype   = NULL;
    zvcache_info         zvinfo      = {0};
    zend_bool            summaryonly = 0;

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &summaryonly) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    plist = (zend_llist *)alloc_emalloc(sizeof(zend_llist));
    if(plist == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    result = zvcache_list(WCG(zvucache), summaryonly, &zvinfo, plist);
    if(FAILED(result))
    {
        goto Finished;
    }

    /* Fill the array and then call zend_llist_destroy */
    array_init(return_value);
    add_assoc_long(return_value, "total_cache_uptime", zvinfo.initage);
    add_assoc_bool(return_value, "is_local_cache", zvinfo.islocal);
    add_assoc_long(return_value, "total_item_count", zvinfo.itemcount);
    add_assoc_long(return_value, "total_hit_count", zvinfo.hitcount);
    add_assoc_long(return_value, "total_miss_count", zvinfo.misscount);

    MAKE_STD_ZVAL(zfentries);
    array_init(zfentries);
    
    peinfo = (zvcache_info_entry *)zend_llist_get_first(plist);
    while(peinfo != NULL)
    {
        MAKE_STD_ZVAL(zfentry);
        array_init(zfentry);

        switch(peinfo->type)
        {
            case IS_NULL:
                valuetype = "null";
                break;
            case IS_BOOL:
                valuetype = "bool";
                break;
            case IS_LONG:
                valuetype = "long";
                break;
            case IS_DOUBLE:
                valuetype = "double";
                break;
            case IS_STRING:
            case IS_CONSTANT:
                valuetype = "string";
                break;
            case IS_ARRAY:
            case IS_CONSTANT_ARRAY:
                valuetype = "array";
                break;
            case IS_OBJECT:
                valuetype = "object";
                break;
            default:
                valuetype = "unknown";
                break;
        }

        add_assoc_string(zfentry, "key_name", peinfo->key, 1);
        add_assoc_string(zfentry, "value_type", valuetype, 1);
        add_assoc_long(zfentry, "value_size", peinfo->sizeb);
        add_assoc_long(zfentry, "ttl_seconds", peinfo->ttl);
        add_assoc_long(zfentry, "age_seconds", peinfo->age);
        add_assoc_long(zfentry, "hitcount", peinfo->hitcount);

        add_index_zval(zfentries, index++, zfentry);
        peinfo = (zvcache_info_entry *)zend_llist_get_next(plist);
    }

    add_assoc_zval(return_value, "ucache_entries", zfentries);

    _ASSERT(SUCCEEDED(result));

Finished:

    if(plist != NULL)
    {
        zend_llist_destroy(plist);
        alloc_efree(plist);
        plist = NULL;
    }

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_info", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }

    return;
}

PHP_FUNCTION(wincache_scache_info)
{
    int                  result      = NONFATAL;
    zend_llist *         plist       = NULL;
    zvcache_info_entry * peinfo      = NULL;
    zval *               zfentries   = NULL;
    zval *               zfentry     = NULL;
    unsigned int         index       = 1;
    char *               valuetype   = NULL;
    zvcache_info         zvinfo      = {0};
    zend_bool            summaryonly = 0;

    if(WCG(zvscache) != NULL)
    {
        if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &summaryonly) == FAILURE)
        {
            result = FATAL_INVALID_ARGUMENT;
            goto Finished;
        }

        plist = (zend_llist *)alloc_emalloc(sizeof(zend_llist));
        if(plist == NULL)
        {
            result = FATAL_OUT_OF_LMEMORY;
            goto Finished;
        }

        result = zvcache_list(WCG(zvscache), summaryonly, &zvinfo, plist);
        if(FAILED(result))
        {
            goto Finished;
        }
    }

    /* Fill the array and then call zend_llist_destroy */
    array_init(return_value);

    /* If cache is not initialized, set properties to 0 */
    if(plist == NULL)
    {
        add_assoc_long(return_value, "total_cache_uptime", 0);
        add_assoc_bool(return_value, "is_local_cache", 0);
        add_assoc_long(return_value, "total_item_count", 0);
        add_assoc_long(return_value, "total_hit_count", 0);
        add_assoc_long(return_value, "total_miss_count", 0);
    }
    else
    {
        add_assoc_long(return_value, "total_cache_uptime", zvinfo.initage);
        add_assoc_bool(return_value, "is_local_cache", zvinfo.islocal);
        add_assoc_long(return_value, "total_item_count", zvinfo.itemcount);
        add_assoc_long(return_value, "total_hit_count", zvinfo.hitcount);
        add_assoc_long(return_value, "total_miss_count", zvinfo.misscount);
    }

    MAKE_STD_ZVAL(zfentries);
    array_init(zfentries);
    
    if(plist != NULL)
    {
        peinfo = (zvcache_info_entry *)zend_llist_get_first(plist);
        while(peinfo != NULL)
        {
            MAKE_STD_ZVAL(zfentry);
            array_init(zfentry);

            switch(peinfo->type)
            {
                case IS_NULL:
                    valuetype = "null";
                    break;
                case IS_BOOL:
                    valuetype = "bool";
                    break;
                case IS_LONG:
                    valuetype = "long";
                    break;
                case IS_DOUBLE:
                    valuetype = "double";
                    break;
                case IS_STRING:
                case IS_CONSTANT:
                    valuetype = "string";
                    break;
                case IS_ARRAY:
                case IS_CONSTANT_ARRAY:
                    valuetype = "array";
                    break;
                case IS_OBJECT:
                    valuetype = "object";
                    break;
                default:
                    valuetype = "unknown";
                    break;
            }

            add_assoc_string(zfentry, "key_name", peinfo->key, 1);
            add_assoc_string(zfentry, "value_type", valuetype, 1);
            add_assoc_long(zfentry, "value_size", peinfo->sizeb);
            add_assoc_long(zfentry, "ttl_seconds", peinfo->ttl);
            add_assoc_long(zfentry, "age_seconds", peinfo->age);
            add_assoc_long(zfentry, "hitcount", peinfo->hitcount);

            add_index_zval(zfentries, index++, zfentry);
            peinfo = (zvcache_info_entry *)zend_llist_get_next(plist);
        }
    }

    add_assoc_zval(return_value, "scache_entries", zfentries);

    _ASSERT(SUCCEEDED(result));

Finished:

    if(plist != NULL)
    {
        zend_llist_destroy(plist);
        alloc_efree(plist);
        plist = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in wincache_scache_info", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        RETURN_FALSE;
    }

    return;
}

PHP_FUNCTION(wincache_ucache_inc)
{
    int          result  = NONFATAL;
    char *       key     = NULL;
    unsigned int keylen  = 0;
    int          delta   = 1;
    int          newval  = 0;
    zval *       success = NULL;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lz", &key, &keylen, &delta, &success) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    if(success != NULL)
    {
        ZVAL_BOOL(success, 0);
    }

    result = zvcache_change(WCG(zvucache), key, delta, &newval);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(success != NULL)
    {
        ZVAL_BOOL(success, 1);
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_inc", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(result == WARNING_ZVCACHE_NOTLONG)
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "function can only be called for key whose value is long");
        }

        RETURN_FALSE;
    }

    RETURN_LONG(newval);
}

PHP_FUNCTION(wincache_ucache_dec)
{
    int          result  = NONFATAL;
    char *       key     = NULL;
    unsigned int keylen  = 0;
    int          delta   = 1;
    int          newval  = 0;
    zval *       success = NULL;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lz", &key, &keylen, &delta, &success) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    if(success != NULL)
    {
        ZVAL_BOOL(success, 0);
    }

    /* Convert to negative number */
    delta = -delta;

    result = zvcache_change(WCG(zvucache), key, delta, &newval);
    if(FAILED(result))
    {
        goto Finished;
    }

    if(success != NULL)
    {
        ZVAL_BOOL(success, 1);
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_dec", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(result == WARNING_ZVCACHE_NOTLONG)
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "function can only be called for key whose value is long");
        }

        RETURN_FALSE;
    }

    RETURN_LONG(newval);
}

PHP_FUNCTION(wincache_ucache_cas)
{
    int          result = NONFATAL;
    char *       key    = NULL;
    unsigned int keylen = 0;
    int          cvalue = 0;
    int          nvalue = 0;

    /* If user cache is enabled, return false */
    if(!WCG(ucenabled))
    {
        RETURN_FALSE;
    }

    if(WCG(zvucache) == NULL)
    {
        result = FATAL_UNEXPECTED_FCALL;
        goto Finished;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &key, &keylen, &cvalue, &nvalue) == FAILURE)
    {
        result = FATAL_INVALID_ARGUMENT;
        goto Finished;
    }

    result = zvcache_compswitch(WCG(zvucache), key, cvalue, nvalue);
    if(FAILED(result))
    {
        goto Finished;
    }

    _ASSERT(SUCCEEDED(result));

Finished:

    if(FAILED(result))
    {
        if(result == WARNING_ZVCACHE_CASNEQ)
        {
            RETURN_FALSE;
        }

        WCG(uclasterror) = result;

        dprintimportant("failure %d in wincache_ucache_cas", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(result == WARNING_ZVCACHE_NOTLONG)
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "function can only be called for key whose value is long");
        }

        RETURN_FALSE;
    }

    RETURN_TRUE;
}

#ifdef WINCACHE_TEST
PHP_FUNCTION(wincache_ucache_lasterror)
{
    if(WCG(zvucache) == NULL)
    {
        RETURN_LONG(0);
    }

    RETURN_LONG(WCG(uclasterror));
}

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
