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

ps_module ps_mod_wincache = { PS_MOD(wincache) };

/* Called on session start. Nothing to do */
PS_OPEN_FUNC(wincache)
{
    static char dummy = 0;

    dprintverbose("start ps_open_func");

    /* Fool the session module in believing there is a */
    /* valid data associated with session handler */
    PS_SET_MOD_DATA(&dummy);

    dprintverbose("end ps_open_func");
    return SUCCESS;
}

/* Called on session close. Nothing to do */
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
    int    result = NONFATAL;
    zval * pzval  = NULL;

    dprintverbose("start ps_read_func");

    _ASSERT(key    != NULL);
    _ASSERT(val    != NULL);
    _ASSERT(vallen != NULL);

    MAKE_STD_ZVAL(pzval);
    ZVAL_NULL(pzval);

    result = zvcache_get(WCG(zvcache), key, 1, &pzval TSRMLS_CC);
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
        _ASSERT(result > WARNING_COMMON_BASE);


        return FAILURE;
    }

    dprintverbose("end ps_read_func");
    return SUCCESS;
}

/* Called on session close which writes all values to memory */
PS_WRITE_FUNC(wincache)
{
    int    result = NONFATAL;
    zval * pzval  = NULL;

    dprintverbose("start ps_write_func");

    _ASSERT(key != NULL);
    _ASSERT(val != NULL);

    MAKE_STD_ZVAL(pzval);
    ZVAL_STRINGL(pzval, val, vallen, 0);

    /* issession = 1, ttl = session.gc_maxlifetime, isadd = 0 */
    result = zvcache_set(WCG(zvcache), key, 1, pzval, INI_INT("session.gc_maxlifetime"), 0 TSRMLS_CC);
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
        _ASSERT(result > WARNING_COMMON_BASE);

        return FAILURE;
    }

    dprintverbose("end ps_write_func");
    return SUCCESS;
}

/* Called on session_destroy */
PS_DESTROY_FUNC(wincache)
{
    int result = NONFATAL;

    dprintverbose("start ps_destroy_func");

    _ASSERT(key != NULL);

    result = zvcache_delete(WCG(zvcache), key, 1);
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
        _ASSERT(result > WARNING_COMMON_BASE);

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
