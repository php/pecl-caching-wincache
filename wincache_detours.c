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
   | Module: wincache_detours.c                                                                   |
   +----------------------------------------------------------------------------------------------+
   | Author: Kanwaljeet Singla <ksingla@microsoft.com>                                            |
   +----------------------------------------------------------------------------------------------+
*/

#include "precomp.h"

#define FUNCTION_REROUTE          1
#define CLASS_REROUTE             2
#define FUNCTION_SECTION_NAME     "FunctionRerouteList"
#define CLASS_SECTION_NAME        "ClassRerouteList"
#define MAX_SECTION_DATA          32760

static int  ini_readfile(HashTable * hrouter, char * filepath, char * sectionname);
static void ini_parseline(char * line, HashTable * hrouter);
static int  reroute_verify(HashTable * hrouter, unsigned char type);
static int  exists_check(detours_context * pdetours, char * oname, char ** rname, unsigned char type);

/* Private functions */
static int ini_readfile(HashTable * hrouter, char * filepath, char * sectionname)
{
    int          result   = NONFATAL;
    char *       pbuffer  = NULL;
    char *       pcurrent = NULL;
    unsigned int retvalue = 0;
    unsigned int currlen  = 0;

    _ASSERT(hrouter     != NULL);
    _ASSERT(filepath    != NULL);
    _ASSERT(sectionname != NULL);

    /* Read raw section details */
    pbuffer = (char *)alloc_pemalloc(MAX_SECTION_DATA);
    if(pbuffer == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    retvalue = GetPrivateProfileSection(sectionname, pbuffer, MAX_SECTION_DATA, filepath);
    if(retvalue == MAX_SECTION_DATA - 2)
    {
        result = FATAL_DETOURS_TOOBIG;
        goto Finished;
    }

    /* Go through the entries and add them to hrouter */
    pcurrent = pbuffer;
    while(*pcurrent != '\0')
    {
        currlen = strlen(pcurrent);
        ini_parseline(pcurrent, hrouter);
        pcurrent = pcurrent + currlen + 1;
    }

Finished:

    if(pbuffer != NULL)
    {
        alloc_pefree(pbuffer);
        pbuffer = NULL;
    }

    if(FAILED(result))
    {
        dprintimportant("failure %d in ini_readfile", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    return result;
}

static void ini_parseline(char * line, HashTable * hrouter)
{
    char *       key      = NULL;
    unsigned int keylen   = 0;
    char *       value    = NULL;
    unsigned int valuelen = 0;
    char *       pchar    = NULL;

    _ASSERT(line    != NULL);
    _ASSERT(hrouter != NULL);

    pchar = strchr(line, '=');
    if(pchar == NULL)
    {
        return;
    }

    key = line;
    keylen = pchar - line;

    *pchar = '\0';
    line = pchar + 1;

    value = line;

    /* Skip whitespace after before ; or end of line */
    pchar = strchr(line, ';');
    if(pchar == NULL)
    {
        pchar = line + strlen(line) - 1;
    }
    else
    {
        pchar--;
    }

    while(*pchar == ' ' || *pchar == '\t')
    {
        pchar--;
    }

    *(pchar + 1) = '\0';
    valuelen = pchar + 1 - value;

    /* Add this to hashtable. Ignore if entry already exists */
    zend_hash_add(hrouter, key, keylen + 1, value, valuelen + 1, NULL);

    return;
}

static int reroute_verify(HashTable * hrouter, unsigned char type)
{
    int result = NONFATAL;

    /* Function checks */
    /* Number of arguments are same */
    /* Type of arguments is same */
    /* Return type is same */

    /* Class checks */
    /* Number of functions are same */
    /* All functions are present in both classes */
    /* Function arguments and return types match */

    return result;
}

static int exists_check(detours_context * pdetours, char * oname, char ** rname, unsigned char type)
{
    int         result = NONFATAL;
    HashTable * htable = NULL;
    char *      pvalue = NULL;

    _ASSERT(pdetours != NULL);
    _ASSERT(oname    != NULL);
    _ASSERT(rname    != NULL);
    _ASSERT(type == FUNCTION_REROUTE || type == CLASS_REROUTE);

    *rname = NULL;
    htable = ((type == FUNCTION_REROUTE) ? pdetours->fnroutes : pdetours->clroutes);

    /* Find oname in the hashtable */
    if(zend_hash_find(htable, oname, strlen(oname) + 1, (void **)&pvalue) == FAILURE)
    {
        /* result = NONFATAL and rname = NULL indicates not found */
        goto Finished;
    }

    *rname = pvalue;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in exists_check", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    return result;
}

/* Public functions */
int detours_create(detours_context ** ppdetours)
{
    int               result   = NONFATAL;
    detours_context * pdetours = NULL;

    _ASSERT(ppdetours != NULL);

    pdetours = (detours_context *)alloc_pemalloc(sizeof(detours_context));
    if(pdetours == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pdetours->inifile  = NULL;
    pdetours->fnroutes = NULL;
    pdetours->clroutes = NULL;

    *ppdetours = pdetours;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in detours_create", result);
        _ASSERT(result > WARNING_COMMON_BASE);
    }

    return result;
}

void detours_destroy(detours_context * pdetours)
{
    if(pdetours != NULL)
    {
        alloc_pefree(pdetours);
        pdetours = NULL;
    }

    return;
}

int detours_initialize(detours_context * pdetours, char * inifile)
{
    int    result   = NONFATAL;
    int    retvalue = 0;
    char   realpath[  MAX_PATH];

    _ASSERT(pdetours != NULL);
    _ASSERT(inifile  != NULL);
 
    /* Check if ini file exists */
    if(GetFileAttributes(inifile) == 0xFFFFFFFF)
    {
        result = FATAL_DETOURS_INITIALIZE;
        goto Finished;
    }

    /* GetFull file path if only relative path is given */
    if(!IS_ABSOLUTE_PATH(inifile, strlen(inifile)))
    {
        retvalue = GetFullPathName(inifile, MAX_PATH, realpath, NULL);
        if(!retvalue || retvalue > MAX_PATH)
        {
            goto Finished;
        }

        inifile = realpath;
    }

    pdetours->inifile = alloc_pestrdup(inifile);
    if(pdetours->inifile == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    /* Create hashtables and initialize them */
    pdetours->fnroutes = (HashTable *)alloc_pemalloc(sizeof(HashTable));
    if(pdetours->fnroutes == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    zend_hash_init(pdetours->fnroutes, 0, NULL, NULL, 1);
    result = ini_readfile(pdetours->fnroutes, inifile, FUNCTION_SECTION_NAME);
    if(FAILED(result))
    {
        goto Finished;
    }

    pdetours->clroutes = (HashTable *)alloc_pemalloc(sizeof(HashTable));
    if(pdetours->clroutes == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    zend_hash_init(pdetours->clroutes, 0, NULL, NULL, 1);
    result = ini_readfile(pdetours->clroutes, inifile, CLASS_SECTION_NAME);
    if(FAILED(result))
    {
        goto Finished;
    }

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in detours_initialize", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pdetours->inifile != NULL)
        {
            alloc_pefree(pdetours->inifile);
            pdetours->inifile = NULL;
        }

        if(pdetours->fnroutes != NULL)
        {
            zend_hash_destroy(pdetours->fnroutes);
            alloc_pefree(pdetours->fnroutes);
            pdetours->fnroutes = NULL;
        }

        if(pdetours->clroutes != NULL)
        {
            zend_hash_destroy(pdetours->clroutes);
            alloc_pefree(pdetours->clroutes);
            pdetours->clroutes = NULL;
        }
    }

    return result;
}

void detours_terminate(detours_context * pdetours)
{
    if(pdetours != NULL)
    {
        if(pdetours->inifile != NULL)
        {
            alloc_pefree(pdetours->inifile);
            pdetours->inifile = NULL;
        }

        if(pdetours->fnroutes != NULL)
        {
            zend_hash_destroy(pdetours->fnroutes);
            alloc_pefree(pdetours->fnroutes);
            pdetours->fnroutes = NULL;
        }

        if(pdetours->clroutes != NULL)
        {
            zend_hash_destroy(pdetours->clroutes);
            alloc_pefree(pdetours->clroutes);
            pdetours->clroutes = NULL;
        }
    }

    return;
}

int detours_fcheck(detours_context * pdetours, char * ofname, char ** rfname)
{
    return exists_check(pdetours, ofname, rfname, FUNCTION_REROUTE);
}

int detours_ccheck(detours_context * pdetours, char * ocname, char ** rcname)
{
    return exists_check(pdetours, ocname, rcname, CLASS_REROUTE);
}

int detours_getinfo(detours_context * pdetours, detours_info ** ppinfo)
{
    int            result   = NONFATAL;
    detours_info * pinfo    = NULL;
    HashTable *    htable   = NULL;
    char *         pkey     = NULL;
    char *         pvalue   = NULL;
    dlist_element  delement = {0};

    _ASSERT(pdetours != NULL);
    _ASSERT(ppinfo   != NULL);

    pinfo = (detours_info *)alloc_emalloc(sizeof(detours_info));
    if(pinfo == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    pinfo->fnlist = NULL;
    pinfo->cllist = NULL;

    pinfo->fnlist = (zend_llist *)alloc_emalloc(sizeof(zend_llist));
    if(pinfo->fnlist == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    zend_llist_init(pinfo->fnlist, sizeof(dlist_element), NULL, 0);

    htable = pdetours->fnroutes;
    zend_hash_internal_pointer_reset(htable);

    while(zend_hash_has_more_elements(htable) == SUCCESS)
    {
        zend_hash_get_current_key(htable, &pkey, NULL, 0);
        zend_hash_get_current_data(htable, (void **)&pvalue);

        delement.otname = pkey;
        delement.rtname = pvalue;
        zend_llist_add_element(pinfo->fnlist, &delement);

        zend_hash_move_forward(htable);
    }

    pinfo->cllist = (zend_llist *)alloc_emalloc(sizeof(zend_llist));
    if(pinfo->cllist == NULL)
    {
        result = FATAL_OUT_OF_LMEMORY;
        goto Finished;
    }

    zend_llist_init(pinfo->cllist, sizeof(dlist_element), NULL, 0);

    htable = pdetours->clroutes;
    zend_hash_internal_pointer_reset(htable);

    while(zend_hash_has_more_elements(htable) == SUCCESS)
    {
        zend_hash_get_current_key(htable, &pkey, NULL, 0);
        zend_hash_get_current_data(htable, (void **)&pvalue);

        delement.otname = pkey;
        delement.rtname = pvalue;
        zend_llist_add_element(pinfo->cllist, &delement);

        zend_hash_move_forward(htable);
    }

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in detours_getinfo", result);
        _ASSERT(result > WARNING_COMMON_BASE);

        if(pinfo != NULL)
        {
            if(pinfo->fnlist != NULL)
            {
                zend_llist_destroy(pinfo->fnlist);
                alloc_efree(pinfo->fnlist);
                pinfo->fnlist = NULL;
            }

            if(pinfo->cllist != NULL)
            {
                zend_llist_destroy(pinfo->cllist);
                alloc_efree(pinfo->cllist);
                pinfo->cllist = NULL;
            }

            alloc_efree(pinfo);
            pinfo = NULL;
        }
    }

    return result;
}

void detours_freeinfo(detours_info * pinfo)
{
    if(pinfo != NULL)
    {
        if(pinfo->fnlist != NULL)
        {
            zend_llist_destroy(pinfo->fnlist);
            alloc_efree(pinfo->fnlist);
            pinfo->fnlist = NULL;
        }

        if(pinfo->cllist != NULL)
        {
            zend_llist_destroy(pinfo->cllist);
            alloc_efree(pinfo->cllist);
            pinfo->cllist = NULL;
        }

        alloc_efree(pinfo);
        pinfo = NULL;
    }
}
