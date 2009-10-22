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
   | Module: wincache_aplist.h                                                                    |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#ifndef _WINCACHE_APLIST_H_
#define _WINCACHE_APLIST_H_

/* aplist_value   - SHARED */
/* aplist_header  - SHARED */
/* aplist_context - LOCAL */
typedef struct aplist_value aplist_value;
struct aplist_value
{
    size_t          file_path;      /* Full path to file */
    FILETIME        modified_time;  /* Modified time of file */
    unsigned int    attributes;     /* File attributes */
    unsigned int    file_size;      /* File size in bytes */

    unsigned int    add_ticks;      /* Ticks when this entry was created */
    unsigned int    use_ticks;      /* Ticks when this entry was last used */
    unsigned int    last_check;     /* Ticks when last file change check was made */

    size_t          fcacheval;      /* File cache value offset */
    size_t          ocacheval;      /* Opcode cache value offset */
    size_t          relentry;       /* Offset of first entry in rplist */
    size_t          prev_value;     /* previous aplist_value offset */
    size_t          next_value;     /* next aplist_value offset */
};

typedef struct aplist_header aplist_header;
struct aplist_header
{
    unsigned int       mapcount;    /* Number of processes using this file cache */
    unsigned int       init_ticks;  /* Tick count when the cache first got created */
    unsigned int       rdcount;     /* Reader count for shared lock */

    unsigned int       ttlmax;      /* Max time a file can stay w/o being used */
    unsigned int       scfreq;      /* How frequently should scavenger run */
    unsigned int       lscavenge;   /* Ticks when was last time we ran scavenger */
    unsigned int       scstart;     /* Entry starting which scavenger should run */

    unsigned int       itemcount;   /* Number of valid items */
    unsigned int       valuecount;  /* Total values starting from last entry */
    size_t             values[1];   /* valuecount aplist_value offsets */
};

typedef struct aplist_context aplist_context;
struct aplist_context
{
    unsigned short     id;          /* unique identifier for cache */
    unsigned short     islocal;     /* is the cache local or shared */
    HANDLE             hinitdone;   /* event indicating if memory is initialized */
    unsigned int       fchangefreq; /* file change check frequency in mseconds */

    char *             apmemaddr;   /* base addr of memory segment */
    aplist_header *    apheader;    /* aplist cache header */
    filemap_context *  apfilemap;   /* filemap where aplist is kept */
    lock_context *     aprwlock;    /* reader writer lock for aplist header */
    alloc_context *    apalloc;     /* alloc context for aplist segment */

    rplist_context *   prplist;     /* Relative path cache to resolve relative paths */
    fcache_context *   pfcache;     /* File cache containing file content */
    ocache_context *   pocache;     /* Opcode cache containing opcodes */
    int                resnumber;   /* Resource number for this extension */
};

typedef struct cache_entry_info cache_entry_info;
struct cache_entry_info
{
    char *             filename;    /* file name */
    unsigned int       addage;      /* Seconds elapsed after add */
    unsigned int       useage;      /* Seconds elapsed after last use */
    unsigned int       lchkage;     /* Seconds elapsed after last check */
    void *             cdata;       /* Custom data for file/opcode cache */
    cache_entry_info * next;        /* next entry */
};

typedef struct cache_info cache_info;
struct cache_info
{
    unsigned int       initage;     /* Seconds elapsed after cache init */
    unsigned int       itemcount;   /* Total number of items in subcache */
    unsigned int       hitcount;    /* Hit count of subcache */
    unsigned int       misscount;   /* Miss count of subcache */
    cache_entry_info * entries;     /* Individual entries */
};

extern int  aplist_create(aplist_context ** ppcache);
extern void aplist_destroy(aplist_context * pcache);
extern int  aplist_initialize(aplist_context * pcache, unsigned short islocal, unsigned int filecount, unsigned int fchangefreq, unsigned int ttlmax TSRMLS_DC);
extern void aplist_terminate(aplist_context * pcache);

extern int  aplist_getinfo(aplist_context * pcache, unsigned char type, cache_info ** ppinfo);
extern void aplist_freeinfo(unsigned char type, cache_info * pinfo);
extern int  aplist_getentry(aplist_context * pcache, const char * filename, unsigned int findex, aplist_value ** ppvalue);
extern int  aplist_force_fccheck(aplist_context * pcache, zval * filelist TSRMLS_DC);

extern int  aplist_fcache_initialize(aplist_context * plcache, unsigned int size, unsigned int maxfilesize TSRMLS_DC);
extern int  aplist_fcache_get(aplist_context * pcache, const char * filename, char ** ppfullpath, fcache_value ** ppvalue TSRMLS_DC);
extern int  aplist_fcache_use(aplist_context * pcache, const char * fullpath, fcache_value * pvalue, zend_file_handle ** pphandle);
extern void aplist_fcache_close(aplist_context * pcache, fcache_value * pvalue);

extern int  aplist_ocache_initialize(aplist_context * plcache, int resnumber, unsigned int size TSRMLS_DC);
extern int  aplist_ocache_get(aplist_context * pcache, const char * filename, zend_file_handle * file_handle, int type, zend_op_array ** poparray, ocache_value ** ppvalue TSRMLS_DC);
extern int  aplist_ocache_get_value(aplist_context * pcache, const char * filename, ocache_value ** ppvalue);
extern int  aplist_ocache_use(aplist_context * pcache, ocache_value * pvalue, zend_op_array ** pparray TSRMLS_DC);
extern void aplist_ocache_close(aplist_context * pcache, ocache_value * pvalue);

extern void aplist_runtest();

#endif /* _WINCACHE_APLIST_H_ */

