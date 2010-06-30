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
   | Module: wincache_opcopy.h                                                                    |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#ifndef _WINCACHE_OPCOPY_H_
#define _WINCACHE_OPCOPY_H_

#define OPCOPY_OPERATION_INVALID 0
#define OPCOPY_OPERATION_COPYIN  1
#define OPCOPY_OPERATION_COPYOUT 2

#define copy_flag_zval           0x00000001 /* not used */
#define copy_flag_zval_ref       0x00000002 /* copy */
#define copy_flag_zend_func      0x00000004 /* copy, check */
#define copy_flag_def_prop       0x00000008 /* check */
#define copy_flag_prop_info      0x00000010 /* copy, check */
#define copy_flag_stat_var       0x00000020 /* check */
#define copy_flag_pDataPtr       0x00000040 /* copy */
#define copy_flag_docheck        0x00000080 /* check */

#define oparray_has_const        0x00000001
#define oparray_has_jumps        0x00000002

typedef struct opcopy_context opcopy_context;
struct opcopy_context
{
    /* Used for copying opcode to shared memory */
    unsigned int    oldfncount; /* Function count before we did compile file */
    unsigned int    oldclcount; /* Class count before we did compile file */
    unsigned int    oldtncount; /* Constant count before we did compile file */
    unsigned int    newfncount; /* New function count */
    unsigned int    newclcount; /* New class count */
    unsigned int    newtncount; /* New constant count */
    int             resnumber;  /* resource number to use */
    zend_op_array * oparray;    /* Opcode array to copy */

    /* Copying opcode from shared memory to local memory */
    zend_op_array * cacheopa;   /* Opcode array from cache */

    /* Memory allocation functions */
    unsigned int    optype;     /* Type of operation in progress */
    unsigned int    oomcode;    /* Out of memory error code to use */
    void *          palloc;     /* Allocator for alloc_a* functions */
    size_t          hoffset;    /* Offset of mpool_header */
    fn_malloc       fnmalloc;   /* Function to use for malloc */
    fn_realloc      fnrealloc;  /* Function to use for realloc */
    fn_strdup       fnstrdup;   /* Function to use for strdup */
    fn_free         fnfree;     /* Function to use for free */
};

extern int  opcopy_create(opcopy_context ** ppopcopy);
extern void opcopy_destroy(opcopy_context * popcopy);
extern int  opcopy_initialize(opcopy_context * popcopy, alloc_context * palloc, size_t hoffset, int resnumber);
extern void opcopy_terminate(opcopy_context * popcopy);

extern int  opcopy_zend_copyin(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);
extern int  opcopy_zend_copyout(opcopy_context * popcopy, ocache_value * pvalue TSRMLS_DC);
extern void opcopy_runtest();

#endif /* _WINCACHE_OPCOPY_H_ */
