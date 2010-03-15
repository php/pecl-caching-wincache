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
   | Module: php_wincache.h                                                                       |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#ifndef _PHP_WINCACHE_H_
#define _PHP_WINCACHE_H_

extern zend_module_entry wincache_module_entry;
#define phpext_wincache_ptr &wincache_module_entry

/* For static builds we need all the headers as this */
/* file is included in main/internal_functions.c */
#ifndef COMPILE_DL_WINCACHE
#include "precomp.h"
#endif

typedef struct ocacheval_list ocacheval_list;
struct ocacheval_list
{
    ocache_value *   pvalue; /* ocache value which is in use */
    ocacheval_list * next;   /* pointer to next ocacheval_list entry */
};

/* Module globals */
ZEND_BEGIN_MODULE_GLOBALS(wincache)
    aplist_context *         lfcache;     /* Shared memory for fcache filelist */
    aplist_context *         locache;     /* Shared memory for ocache filelist */
    zvcache_context *        zvucache;    /* User controlled user cache */
    zvcache_context *        zvscache;    /* Zval cache used to store session data */
    unsigned char            issame;      /* Is the opcode cache local */
    unsigned int             numfiles;    /* Configured number of files to handle */
    unsigned int             fcchkfreq;   /* File change check frequency in seconds */
    unsigned int             ttlmax;      /* Seconds a cache entry can stay dormant */

    zend_bool                enablecli;   /* Enable wincache for command line sapi */
    zend_bool                fcenabled;   /* File cache enabled or disabled */
    unsigned int             fcachesize;  /* File cache size in MBs */
    unsigned int             maxfilesize; /* Max file size (kb) allowed in fcache */
    zend_bool                ocenabled;   /* Opcode cache enabled or disabled */
    unsigned int             ocachesize;  /* Opcode cache size in MB */
    zend_bool                ucenabled;   /* User cache enabled or disabled */
    unsigned int             ucachesize;  /* User cache size in MBs */
    unsigned int             scachesize;  /* Session cache size in MBs */
    unsigned int             debuglevel;  /* Debug dump level (0/101/201/301/401/501) */
    char *                   ignorelist;  /* Pipe-separated list of files to ignore */
    char *                   ocefilter;   /* Comma-separated sitelist having ocenabled toggled */
    char *                   fcefilter;   /* Comma-separated sitelist having fcenabled toggled */
    char *                   namesalt;    /* Salt to use in all the names */
    zend_bool                localheap;   /* Local heap is enabled or disabled */

    ocacheval_list *         oclisthead;  /* List of ocache_value entries in use */
    ocacheval_list *         oclisttail;  /* Tail of ocache_value entries list */
    unsigned int             lasterror;   /* Last error value */
    unsigned int             uclasterror; /* Last error value encountered by user cache */
    unsigned int             parentpid;   /* Parent process identifier */
    filemap_global_context * fmapgdata;   /* Global data for filemap */
    zend_llist *             errmsglist;  /* List of errors generated by PHP */
    unsigned char            dooctoggle;  /* Do toggle of ocenabled due to filter settigns */
    unsigned char            dofctoggle;  /* Do toggle of fcenabled due to filter settigns */
#ifdef WINCACHE_TEST
    zend_bool                olocaltest;  /* Local opcode cache test configuration */
#endif
ZEND_END_MODULE_GLOBALS(wincache)

ZEND_EXTERN_MODULE_GLOBALS(wincache)

#ifdef ZTS
#define WCG(v) TSRMG(wincache_globals_id, zend_wincache_globals *, v)
#else
#define WCG(v) (wincache_globals.v)
#endif

typedef char *(*fn_zend_resolve_path)(const char *filename, int filename_len TSRMLS_DC);
typedef int (*fn_zend_stream_open_function)(const char * filename, zend_file_handle *handle TSRMLS_DC);
typedef zend_op_array * (*fn_zend_compile_file)(zend_file_handle *, int TSRMLS_DC);
typedef void (*fn_zend_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

fn_zend_resolve_path original_resolve_path;
fn_zend_stream_open_function original_stream_open_function;
fn_zend_compile_file original_compile_file;
fn_zend_error_cb original_error_cb;

extern char * wincache_resolve_path(const char * filename, int filename_len TSRMLS_DC);
extern int wincache_stream_open_function(const char * filename, zend_file_handle * file_handle TSRMLS_DC);
extern zend_op_array * wincache_compile_file(zend_file_handle * file_handle, int type TSRMLS_DC);
extern void wincache_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

#endif /* _PHP_WINCACHE_H_ */
