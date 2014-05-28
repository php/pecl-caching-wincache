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
   | Module: wincache_ocache.h                                                                    |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#ifndef _WINCACHE_OCACHE_H_
#define _WINCACHE_OCACHE_H_

/* ocache_header   - SHARED */
/* ocache_value    - SHARED */
/* ocache_context  - LOCAL */

typedef struct ocache_user_function ocache_user_function;
struct ocache_user_function
{
    char *             fname;     /* function name */
    unsigned int       fnamelen;  /* function name length */
    zend_function *    fentry;    /* zend_function pointer */
};

typedef struct ocache_user_class ocache_user_class;
struct ocache_user_class
{
    char *             cname;     /* class name used as key */
    unsigned int       cnamelen;  /* key class name length */
    char *             pcname;    /* parent class name */
    zend_class_entry * centry;    /* zend_class_entry pointer */
};

typedef struct ocache_user_constant ocache_user_constant;
struct ocache_user_constant
{
    char *             tname;     /* constant name */
    unsigned int       tnamelen;  /* constant name length */
    zend_constant *    tentry;    /* zend_constant pointer */
};

typedef struct ocache_user_aglobal ocache_user_aglobal;
struct ocache_user_aglobal
{
    char *             aname;     /* constant name */
    unsigned int       anamelen;  /* constant name length */
};

typedef struct ocache_user_message ocache_user_message;
struct ocache_user_message
{
    int                type;      /* type of error generated */
    char *             filename;  /* filename */
    unsigned int       lineno;    /* line number */
    char *             message;   /* message with args replaced */
};

typedef struct ocache_value ocache_value;
struct ocache_value
{
    size_t                 hoffset;    /* Offset of shared memory pool header */
    zend_op_array *        oparray;    /* zend_op_array pointer for the file */
    ocache_user_function * functions;  /* array of zend functions */
    unsigned int           fcount;     /* count of functions */
    ocache_user_class *    classes;    /* array of zend classes */
    unsigned int           ccount;     /* count of classes */
    ocache_user_constant * constants;  /* array of zend constants */
    unsigned int           tcount;     /* count of constants */

    ocache_user_aglobal *  aglobals;   /* array of auto globals used */
    unsigned int           acount;     /* count of auto globals */
    ocache_user_message *  messages;   /* array of messages generated */
    unsigned int           mcount;     /* count of messages */

    unsigned int           is_deleted; /* if 1, this entry is marked for deletion */
    unsigned int           hitcount;   /* hitcount for this entry */
    unsigned int           refcount;   /* refcount for this value */
};

typedef struct ocache_header ocache_header;
struct ocache_header
{
    unsigned int       mapcount;  /* How many times shared memory is mapped */
    unsigned int       itemcount; /* Number of items */
    unsigned int       hitcount;  /* Cache hit count */
    unsigned int       misscount; /* Cache miss count */
};

typedef struct ocache_context ocache_context;
struct ocache_context
{
    unsigned int       id;        /* unique identifier for cache */
    unsigned short     islocal;   /* is cache local to process */
    unsigned short     cachekey;  /* unique cache key used in names */
    HANDLE             hinitdone; /* event to indicate memory is initialized */
    int                resnumber; /* resource number to use */
    char *             memaddr;   /* base memory address of segment */
    ocache_header *    header;    /* cache header */
    filemap_context *  pfilemap;  /* filemap where opcodes are kept */
    lock_context *     prwlock;   /* writer lock for opcode cache */
    alloc_context *    palloc;    /* alloc context for opcode segment */
};

typedef struct ocache_entry_info ocache_entry_info;
struct ocache_entry_info
{
    unsigned int       hitcount;  /* hitcount for this entry */
    unsigned int       fcount;    /* Number of functions in the entry */
    unsigned int       ccount;    /* Number of classes in the entry */
};

extern int  ocache_create(ocache_context ** pcache);
extern void ocache_destroy(ocache_context * pcache);
extern int  ocache_initialize(ocache_context * pcache, unsigned short islocal, unsigned short cachekey, int resnumber, unsigned int cachesize TSRMLS_DC);
extern void ocache_terminate(ocache_context * pcache);

extern int  ocache_createval(ocache_context * pcache, const char * filename, zend_file_handle * file_handle, int type, zend_op_array ** poparray, ocache_value ** ppvalue TSRMLS_DC);
extern void ocache_destroyval(ocache_context * pcache, ocache_value * pvalue);
extern int  ocache_useval(ocache_context * pcache, ocache_value * pvalue, zend_op_array ** pparray TSRMLS_DC);
extern ocache_value * ocache_getvalue(ocache_context * pocache, size_t offset);
extern size_t ocache_getoffset(ocache_context * pocache, ocache_value * pvalue);
extern void ocache_refinc(ocache_context * pocache, ocache_value * pvalue);
extern void ocache_refdec(ocache_context * pocache, ocache_value * pvalue);
extern int  ocache_getinfo(ocache_value * pvalue, ocache_entry_info ** ppinfo);
extern void ocache_freeinfo(ocache_entry_info * pinfo);

extern void ocache_runtest();

#endif /* _WINCACHE_OCACHE_H_ */
