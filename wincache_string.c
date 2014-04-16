/*
   +----------------------------------------------------------------------------------------------+
   | Windows Cache for PHP                                                                        |
   +----------------------------------------------------------------------------------------------+
   | Copyright (c) 2012, Microsoft Corporation. All rights reserved.                              |
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
   | Module: wincache_string.h                                                                    |
   +----------------------------------------------------------------------------------------------+
   | Authors: Dmitry Stogov <dmitry@zend.com>                                                     |
   |          Pierre Joye <v-pijoye@microsoft.com>                                                |
   |          Eric Stenson <ericsten@microsoft.com>                                               |
   +----------------------------------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.4.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of Microsoft Corporation.

 */

#include "precomp.h"

#ifdef ZEND_ENGINE_2_4

#define ONE_MEGABYTE (1024 * 1024)

#if !defined(ZTS)
typedef struct _wincache_interned_strings_data_t {
    char *interned_strings_start;
    char *interned_strings_end;
    char *interned_strings_top;
#ifdef ZEND_ENGINE_2_6_1
    char *interned_empty_string;
#endif
    lock_context *lock;
    HashTable interned_strings;
} wincache_interned_strings_data_t;

wincache_interned_strings_data_t *wincache_interned_strings_data = NULL;

/* Initialization Sentry */
static int wincache_string_initialized = 0;

#define WCSG(v) (wincache_interned_strings_data->v)

static char *old_interned_strings_start;
static char *old_interned_strings_end;
#ifdef ZEND_ENGINE_2_6_1
static char *old_interned_empty_string;
#endif
static const char *(*old_new_interned_string)(const char *str, int len, int free_src TSRMLS_DC);
static void (*old_interned_strings_snapshot)(TSRMLS_D);
static void (*old_interned_strings_restore)(TSRMLS_D);

static const char *wincache_dummy_new_interned_string_for_php(const char *str, int len, int free_src TSRMLS_DC)
{
    return str;
}

static void wincache_dummy_interned_strings_snapshot_for_php(TSRMLS_D)
{
}

static void wincache_dummy_interned_strings_restore_for_php(TSRMLS_D)
{
}
#endif /* !defined(ZTS) */

const char *wincache_new_interned_string(const char *arKey, uint nKeyLength TSRMLS_DC)
{
#if !defined(ZTS)
    ulong h;
    uint nIndex;
    Bucket *p;

    if (arKey >= WCSG(interned_strings_start) && arKey < WCSG(interned_strings_end)) {
        return arKey;
    }

    h = zend_inline_hash_func(arKey, nKeyLength);
    nIndex = h & WCSG(interned_strings).nTableMask;

    p = WCSG(interned_strings).arBuckets[nIndex];
    while (p != NULL) {
        if ((p->h == h) && (p->nKeyLength == nKeyLength)) {
            if (!memcmp(p->arKey, arKey, nKeyLength)) {
                /* Woo Hoo!  Found an existing string! */
                return p->arKey;
            }
        }
        p = p->pNext;
    }

    if (WCSG(interned_strings_top) + ZEND_MM_ALIGNED_SIZE(sizeof(Bucket) + nKeyLength) >=
        WCSG(interned_strings_end)) {
        /* no memory */
        dprintimportant("Out of memory in wincache_new_interned_string");
        return NULL;
    }

    p = (Bucket *) WCSG(interned_strings_top);
    WCSG(interned_strings_top) += ZEND_MM_ALIGNED_SIZE(sizeof(Bucket) + nKeyLength);

    p->arKey = (char*)(p+1);
    memcpy((char*)p->arKey, arKey, nKeyLength);
    p->nKeyLength = nKeyLength;
    p->h = h;
    p->pData = &p->pDataPtr;
    p->pDataPtr = p;

    p->pNext = WCSG(interned_strings).arBuckets[nIndex];
    p->pLast = NULL;
    if (p->pNext) {
        p->pNext->pLast = p;
    }
    WCSG(interned_strings).arBuckets[nIndex] = p;

    p->pListLast = WCSG(interned_strings).pListTail;
    WCSG(interned_strings).pListTail = p;
    p->pListNext = NULL;
    if (p->pListLast != NULL) {
        p->pListLast->pListNext = p;
    }
    if (!WCSG(interned_strings).pListHead) {
        WCSG(interned_strings).pListHead = p;
    }

    WCSG(interned_strings).nNumOfElements++;

    /* Created new interned string. */
    return p->arKey;
#else /* ZTS */
    return zend_new_interned_string(arKey, nKeyLength, 0 TSRMLS_CC);
#endif /* !defined(ZTS) */
}

#if !defined(ZTS)
static void wincache_copy_internal_strings(TSRMLS_D)
{
    Bucket *p, *q;

    p = CG(function_table)->pListHead;
    while (p) {
        if (p->nKeyLength) {
            p->arKey = wincache_new_interned_string(p->arKey, p->nKeyLength TSRMLS_CC);
        }
        p = p->pListNext;
    }

    p = CG(class_table)->pListHead;
    while (p) {
        zend_class_entry *ce = (zend_class_entry*)(p->pDataPtr);

        if (p->nKeyLength) {
            _ASSERT(strlen(p->arKey) < (size_t)p->nKeyLength);
            p->arKey = wincache_new_interned_string(p->arKey, p->nKeyLength TSRMLS_CC);
        }

        if (ce->name) {
            _ASSERT(strlen(ce->name) < (size_t)ce->name_length+1);
            ce->name = wincache_new_interned_string(ce->name, ce->name_length+1 TSRMLS_CC);
        }

        q = ce->properties_info.pListHead;
        while (q) {
            zend_property_info *info = (zend_property_info*)(q->pData);

            if (q->nKeyLength) {
                q->arKey = wincache_new_interned_string(q->arKey, q->nKeyLength TSRMLS_CC);
            }

            if (info->name) {
                _ASSERT(strlen(info->name) < (size_t)info->name_length+1);
                info->name = wincache_new_interned_string(info->name, info->name_length+1 TSRMLS_CC);
            }

            q = q->pListNext;
        }

        q = ce->function_table.pListHead;
        while (q) {
            if (q->nKeyLength) {
                _ASSERT(strlen(q->arKey) < (size_t)q->nKeyLength);
                q->arKey = wincache_new_interned_string(q->arKey, q->nKeyLength TSRMLS_CC);
            }
            q = q->pListNext;
        }

        q = ce->constants_table.pListHead;
        while (q) {
            if (q->nKeyLength) {
                _ASSERT(strlen(q->arKey) < (size_t)q->nKeyLength);
                q->arKey = wincache_new_interned_string(q->arKey, q->nKeyLength TSRMLS_CC);
            }
            q = q->pListNext;
        }

        p = p->pListNext;
    }

    p = EG(zend_constants)->pListHead;
    while (p) {
        if (p->nKeyLength) {
            _ASSERT(strlen(p->arKey) < (size_t)p->nKeyLength);
            p->arKey = wincache_new_interned_string(p->arKey, p->nKeyLength TSRMLS_CC);
       }
        p = p->pListNext;
    }
}

int wincache_interned_strings_init(TSRMLS_D)
{
    int result = 0;
    size_t interned_cache_size = (WCG(internedsize) * ONE_MEGABYTE);
    int count = interned_cache_size / (sizeof(Bucket) + sizeof(Bucket*) * 2);

    if (wincache_string_initialized) {
        return 0;
    }

    wincache_interned_strings_data = (wincache_interned_strings_data_t*) alloc_pemalloc(interned_cache_size);
    if (wincache_interned_strings_data == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }
    memset((void *)wincache_interned_strings_data, 0, interned_cache_size);

    zend_hash_init(&WCSG(interned_strings), count, NULL, NULL, 1);
    WCSG(interned_strings).nTableMask = WCSG(interned_strings).nTableSize - 1;
    WCSG(interned_strings).arBuckets = (Bucket**)((char*)wincache_interned_strings_data + sizeof(wincache_interned_strings_data_t));

    WCSG(interned_strings_start) = (char*)WCSG(interned_strings).arBuckets + WCSG(interned_strings).nTableSize * sizeof(Bucket *);
    WCSG(interned_strings_end)   = (char*)wincache_interned_strings_data + interned_cache_size;
    WCSG(interned_strings_top)   = WCSG(interned_strings_start);

    old_interned_strings_start = CG(interned_strings_start);
    old_interned_strings_end = CG(interned_strings_end);
    old_new_interned_string = zend_new_interned_string;
    old_interned_strings_snapshot = zend_interned_strings_snapshot;
    old_interned_strings_restore = zend_interned_strings_restore;
    CG(interned_strings_start) = WCSG(interned_strings_start);
    CG(interned_strings_end) = WCSG(interned_strings_end);
    zend_new_interned_string = wincache_dummy_new_interned_string_for_php;
    zend_interned_strings_snapshot = wincache_dummy_interned_strings_snapshot_for_php;
    zend_interned_strings_restore = wincache_dummy_interned_strings_restore_for_php;

#ifdef ZEND_ENGINE_2_6_1
    /* empty string */
    old_interned_empty_string = CG(interned_empty_string);
    CG(interned_empty_string) = (char *)wincache_new_interned_string("", sizeof("") TSRMLS_CC);
    WCSG(interned_empty_string) = CG(interned_empty_string);
#endif

    wincache_copy_internal_strings(TSRMLS_C);

    /*
     * Remember that we successfully initialized, and that we need to
     * cleanup on shutdown.
     */
     wincache_string_initialized = 1;

Finished:
    return result;
}

void wincache_interned_strings_shutdown(TSRMLS_D)
{
    /* Check if we initialized cleanly */
    if (!wincache_string_initialized)
    {
        return;
    }

    zend_hash_clean(CG(function_table));
    zend_hash_clean(CG(class_table));
    zend_hash_clean(EG(zend_constants));

    CG(interned_strings_start) = old_interned_strings_start;
    CG(interned_strings_end) = old_interned_strings_end;
    zend_new_interned_string = old_new_interned_string;
    zend_interned_strings_snapshot = old_interned_strings_snapshot;
    zend_interned_strings_restore = old_interned_strings_restore;

#ifdef ZEND_ENGINE_2_6_1
    /* empty string */
    CG(interned_empty_string) = old_interned_empty_string;
#endif

    /* Remember that we've shutdown cleanly */
    wincache_string_initialized = 0;
}
#endif /* !defined(ZTS) */

#endif /* ZEND_ENGINE_2_4 */

