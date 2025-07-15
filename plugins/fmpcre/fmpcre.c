/*
 * Prototype PCRE function module for rsyslog
 *
 * Copyright 2024 rsyslog
 * Licensed under the Apache License, Version 2.0
 * see COPYING.ASL20 in the source distribution
 */
#include "config.h"
#include <stdint.h>
#include <stddef.h>
#ifndef _AIX
#include <typedefs.h>
#endif
#include <sys/types.h>
#include <pcre.h>
#include <string.h>

#include "rsyslog.h"
#include "parserif.h"
#include "module-template.h"
#include "rainerscript.h"

MODULE_TYPE_FUNCTION
MODULE_TYPE_NOKEEP
DEF_FMOD_STATIC_DATA

struct pcre_state {
        pcre *re;
};

static void ATTR_NONNULL()
doFunc_pcre_match(struct cnffunc *__restrict__ const func,
        struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr,
        wti_t *__restrict__ const pWti)
{
        struct pcre_state *state = (struct pcre_state*)func->funcdata;
        struct svar srcVal;
        int bMustFree;
        char *str;
        int rc;

        cnfexprEval(func->expr[0], &srcVal, usrptr, pWti);
        str = (char*) var2CString(&srcVal, &bMustFree);
        rc = pcre_exec(state->re, NULL, str, strlen(str), 0, 0, NULL, 0);
        if(rc >= 0)
                ret->d.n = 1;
        else
                ret->d.n = 0;
        ret->datatype = 'N';
        if(bMustFree)
                free(str);
        varFreeMembers(&srcVal);
}

static rsRetVal ATTR_NONNULL(1)
initFunc_pcre_match(struct cnffunc *const func)
{
        DEFiRet;
        const char *err;
        int erroffset;
       char *regex = NULL;
        struct pcre_state *state;

        if(func->nParams < 2) {
                parser_errmsg("rsyslog logic error in line %d of file %s", __LINE__, __FILE__);
                ABORT_FINALIZE(RS_RET_ERR);
        }
        if(func->expr[1]->nodetype != 'S') {
                parser_errmsg("param 2 of pcre_match() must be a constant string");
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        CHKmalloc(state = calloc(1, sizeof(struct pcre_state)));
        regex = es_str2cstr(((struct cnfstringval*) func->expr[1])->estr, NULL);
        state->re = pcre_compile(regex, 0, &err, &erroffset, NULL);
        if(state->re == NULL) {
                parser_errmsg("pcre compilation failed at offset %d: %s", erroffset, err);
                free(state);
                ABORT_FINALIZE(RS_RET_ERR);
        }
        func->funcdata = state;
        func->destructable_funcdata = 1;

finalize_it:
        free(regex);
        RETiRet;
}

static void ATTR_NONNULL(1)
destruct_pcre(struct cnffunc *const func)
{
        struct pcre_state *state = (struct pcre_state*)func->funcdata;
        if(state != NULL) {
                if(state->re != NULL)
                        pcre_free(state->re);
                free(state);
        }
}

static struct scriptFunct functions[] = {
        { "pcre_match", 2, 2, doFunc_pcre_match, initFunc_pcre_match, destruct_pcre },
        { NULL, 0, 0, NULL, NULL, NULL }
};

BEGINgetFunctArray
CODESTARTgetFunctArray
        *version = 1;
        *functArray = functions;
ENDgetFunctArray

BEGINmodExit
CODESTARTmodExit
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_FMOD_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
        *ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
        dbgprintf("rsyslog fmpcre init called, compiled with version %s\n", VERSION);
ENDmodInit


