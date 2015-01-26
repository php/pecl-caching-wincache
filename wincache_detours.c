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

#define FUNCTION_SECTION_NAME     "FunctionRerouteList"
#define MAX_SECTION_DATA          32760

static int  ini_readfile(HashTable * hrouter, char * filepath, char * sectionname);
static void ini_parseline(char * line, HashTable * hrouter);

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
    }

    return result;
}

static void ini_parseline(char * line, HashTable * hrouter)
{
    char *          key      = NULL;
    unsigned int    keylen   = 0;
    char *          numarg   = NULL;
    int             numargv  = -1;
    char *          value    = NULL;
    unsigned int    valuelen = 0;
    char *          pchar    = NULL;
    fnroute_element element  = {0};

    _ASSERT(line    != NULL);
    _ASSERT(hrouter != NULL);

    /* Format of each entry is oldcall : arghandled = newcall */
    pchar = strchr(line, '=');
    if(pchar == NULL)
    {
        return;
    }

    key = line;
    keylen = pchar - line;

    *pchar = '\0';
    line = pchar + 1;

    /* Check if : is present in keyname */
    pchar = strchr(key, ':');
    if(pchar != NULL)
    {
        numarg = pchar + 1;

        /* Remove whitespace before colon if present */
        pchar--;
        while(*pchar == ' ' || *pchar == '\t')
        {
            pchar--;
        }
        *(pchar + 1) = '\0';
        keylen = pchar + 1 - key;

        numargv = atoi(numarg);
        if(numargv == 0 && errno == EINVAL)
        {
            return;
        }
    }

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

    element.acount = numargv;
    element.rtname = alloc_pestrdup(value);
    if(element.rtname == NULL)
    {
        return;
    }

    /* Add this to hashtable. Ignore if entry already exists */
    zend_hash_add(hrouter, key, keylen + 1, &element, sizeof(fnroute_element), NULL);

    return;
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

    *ppdetours = pdetours;

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in detours_create", result);
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

static void fnroutes_destructor(void * pdestination)
{
    fnroute_element * pelement = NULL;

    _ASSERT(pdestination != NULL);
    pelement = (fnroute_element *)pdestination;

    _ASSERT(pelement->rtname != NULL);
    alloc_pefree(pelement->rtname);
    pelement->rtname = NULL;

    return;
}


int detours_initialize(detours_context * pdetours, char * inifile)
{
    int    result   = NONFATAL;
    int    retvalue = 0;
    char   realpath[  MAX_PATH];

    _ASSERT(pdetours != NULL);
    _ASSERT(inifile  != NULL);

    dprintverbose("start detours_initialize");

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

    zend_hash_init(pdetours->fnroutes, 0, NULL, fnroutes_destructor, 1);
    result = ini_readfile(pdetours->fnroutes, inifile, FUNCTION_SECTION_NAME);
    if(FAILED(result))
    {
        goto Finished;
    }

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in detours_initialize", result);

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
    }

    dprintverbose("end detours_initialize");

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
    }

    return;
}

int detours_check(detours_context * pdetours, char * oname, int evalue, char ** rname)
{
    int               result = NONFATAL;
    HashTable *       htable = NULL;
    fnroute_element * pvalue = NULL;

    _ASSERT(pdetours != NULL);
    _ASSERT(oname    != NULL);
    _ASSERT(rname    != NULL);

    *rname = NULL;
    htable = pdetours->fnroutes;

    /* Find oname in the hashtable */
    if(zend_hash_find(htable, oname, strlen(oname) + 1, (void **)&pvalue) == FAILURE)
    {
        /* result = NONFATAL and rname = NULL indicates not found */
        goto Finished;
    }

    if(pvalue->acount == -1 || pvalue->acount >= evalue)
    {
        *rname = pvalue->rtname;
    }

Finished:

    if(FAILED(result))
    {
        dprintimportant("failure %d in detours_check", result);
    }

    return result;
}

int detours_getinfo(detours_context * pdetours, HashTable ** pphtable)
{
    _ASSERT(pdetours != NULL);
    _ASSERT(pphtable != NULL);

    *pphtable = pdetours->fnroutes;
    return NONFATAL;
}

void detours_freeinfo(HashTable * phtable)
{
    /* Nothing to do */
}
