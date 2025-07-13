/* ffaup.c
 * This is a function module for URL parsing.
 *
 * File begun on 2021/10/23 by TBertin
 *
 * Copyright 2007-2021 Theo Bertin for Advens
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stddef.h>
#ifndef _AIX
#include <typedefs.h>
#endif
#include <sys/types.h>
#include <string.h>
#include <faup/faup.h>
#include <faup/decode.h>
#include <faup/options.h>
#include <faup/output.h>

#include "config.h"
#include "rsyslog.h"
#include "parserif.h"
#include "module-template.h"
#include "rainerscript.h"



MODULE_TYPE_FUNCTION
MODULE_TYPE_NOKEEP
DEF_FMOD_STATIC_DATA

faup_options_t *glbOptions = NULL;

enum _faup_parse_type_t {
    FAUP_PARSE_ALL,
    FAUP_PARSE_SCHEME,
    FAUP_PARSE_CREDENTIAL,
    FAUP_PARSE_SUBDOMAIN,
    FAUP_PARSE_DOMAIN,
    FAUP_PARSE_DOMAIN_WITHOUT_TLD,
    FAUP_PARSE_HOST,
    FAUP_PARSE_TLD,
    FAUP_PARSE_PORT,
    FAUP_PARSE_RESOURCE_PATH,
    FAUP_PARSE_QUERY_STRING,
    FAUP_PARSE_FRAGMENT
};
typedef enum _faup_parse_type_t faup_parse_type_t;


static inline sbool check_param_count_faup(unsigned short nParams) {
    return nParams != 1;
}


static void ATTR_NONNULL()
do_faup_parse(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti, const faup_parse_type_t parse_type) {
    struct svar srcVal;
    int bMustFree;
    cnfexprEval(func->expr[0], &srcVal, usrptr, pWti);
    char *url = (char*) var2CString(&srcVal, &bMustFree);

    /*
    We use the faup_handler_t struct directly instead of calling faup_init to avoid overhead and useless
    allocations. Using one handler is not possible as the lib is not thread-safe and thread-locale solutions
    still cause some issues.
    If the faup_init function changes significantly in the future, it may cause issue.
    (Validated with faup v1.5 and faup-master fecf768603e713bc903c56c8df0870fae14e3f93)
    */
    faup_handler_t fh = {0};
    fh.options = glbOptions;

    // default, return 0
    ret->datatype = 'N';
    ret->d.n = 0;

    if(!faup_decode(&fh, url, strlen(url))) {
        parser_errmsg("faup: could not parse the value\n");
        // No returned error code, so the reason doesn't matter
        FINALIZE;
    }

    switch(parse_type) {
        case FAUP_PARSE_ALL:
            ret->datatype = 'J';
            ret->d.json = json_object_new_object();
            json_object_object_add(
                ret->d.json,
                "scheme",
                json_object_new_string_len(
                    url + faup_get_scheme_pos(&fh),
                    faup_get_scheme_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "credential",
                json_object_new_string_len(
                    url + faup_get_credential_pos(&fh),
                    faup_get_credential_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "subdomain",
                json_object_new_string_len(
                    url + faup_get_subdomain_pos(&fh),
                    faup_get_subdomain_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "domain",
                json_object_new_string_len(
                    url + faup_get_domain_pos(&fh),
                    faup_get_domain_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "domain_without_tld",
                json_object_new_string_len(
                    url + faup_get_domain_without_tld_pos(&fh),
                    faup_get_domain_without_tld_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "host",
                json_object_new_string_len(
                    url + faup_get_host_pos(&fh),
                    faup_get_host_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "tld",
                json_object_new_string_len(
                    url + faup_get_tld_pos(&fh),
                    faup_get_tld_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "port",
                json_object_new_string_len(
                    url + faup_get_port_pos(&fh),
                    faup_get_port_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "resource_path",
                json_object_new_string_len(
                    url + faup_get_resource_path_pos(&fh),
                    faup_get_resource_path_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "query_string",
                json_object_new_string_len(
                    url + faup_get_query_string_pos(&fh),
                    faup_get_query_string_size(&fh)));
            json_object_object_add(
                ret->d.json,
                "fragment",
                json_object_new_string_len(
                    url + faup_get_fragment_pos(&fh),
                    faup_get_fragment_size(&fh)));
            break;
        case FAUP_PARSE_SCHEME:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_scheme_pos(&fh), faup_get_scheme_size(&fh));
            break;
        case FAUP_PARSE_CREDENTIAL:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_credential_pos(&fh), faup_get_credential_size(&fh));
            break;
        case FAUP_PARSE_SUBDOMAIN:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_subdomain_pos(&fh), faup_get_subdomain_size(&fh));
            break;
        case FAUP_PARSE_DOMAIN:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_domain_pos(&fh), faup_get_domain_size(&fh));
            break;
        case FAUP_PARSE_DOMAIN_WITHOUT_TLD:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_domain_without_tld_pos(&fh), faup_get_domain_without_tld_size(&fh));
            break;
        case FAUP_PARSE_HOST:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_host_pos(&fh), faup_get_host_size(&fh));
            break;
        case FAUP_PARSE_TLD:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_tld_pos(&fh), faup_get_tld_size(&fh));
            break;
        case FAUP_PARSE_PORT:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_port_pos(&fh), faup_get_port_size(&fh));
            break;
        case FAUP_PARSE_RESOURCE_PATH:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_resource_path_pos(&fh), faup_get_resource_path_size(&fh));
            break;
        case FAUP_PARSE_QUERY_STRING:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_query_string_pos(&fh), faup_get_query_string_size(&fh));
            break;
        case FAUP_PARSE_FRAGMENT:
            ret->datatype = 'S';
            ret->d.estr = es_newStrFromCStr(
                url + faup_get_fragment_pos(&fh), faup_get_fragment_size(&fh));
            break;
        default:break;
    }

finalize_it:
    if(bMustFree) {
        free(url);
    }
    varFreeMembers(&srcVal);
}


static void ATTR_NONNULL()
do_faup_parse_full(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_ALL);
}


static void ATTR_NONNULL()
do_faup_parse_scheme(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_SCHEME);
}


static void ATTR_NONNULL()
do_faup_parse_credential(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_CREDENTIAL);
}


static void ATTR_NONNULL()
do_faup_parse_subdomain(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_SUBDOMAIN);
}


static void ATTR_NONNULL()
do_faup_parse_domain(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_DOMAIN);
}


static void ATTR_NONNULL()
do_faup_parse_domain_without_tld(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_DOMAIN_WITHOUT_TLD);
}


static void ATTR_NONNULL()
do_faup_parse_host(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_HOST);
}


static void ATTR_NONNULL()
do_faup_parse_tld(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_TLD);
}


static void ATTR_NONNULL()
do_faup_parse_port(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_PORT);
}


static void ATTR_NONNULL()
do_faup_parse_resource_path(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_RESOURCE_PATH);
}


static void ATTR_NONNULL()
do_faup_parse_query_string(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_QUERY_STRING);
}


static void ATTR_NONNULL()
do_faup_parse_fragment(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
        void *__restrict__ const usrptr, wti_t *__restrict__ const pWti) {
    do_faup_parse(func, ret, usrptr, pWti, FAUP_PARSE_FRAGMENT);
}


static rsRetVal ATTR_NONNULL(1)
initFunc_faup_parse(struct cnffunc *const func)
{
    DEFiRet;

    // Rsyslog cannot free the funcdata object,
    // a library-specific freeing function will be used during destruction
    func->destructable_funcdata = 0;
    if(check_param_count_faup(func->nParams)) {
        parser_errmsg("ffaup: ffaup(key) insufficient params.\n");
        ABORT_FINALIZE(RS_RET_INVLD_NBR_ARGUMENTS);
    }

finalize_it:
    RETiRet;
}


static struct scriptFunct functions[] = {
        {"faup", 1, 1, do_faup_parse_full, initFunc_faup_parse, NULL},
        {"faup_scheme", 1, 1, do_faup_parse_scheme, initFunc_faup_parse, NULL},
        {"faup_credential", 1, 1, do_faup_parse_credential, initFunc_faup_parse, NULL},
        {"faup_subdomain", 1, 1, do_faup_parse_subdomain, initFunc_faup_parse, NULL},
        {"faup_domain", 1, 1, do_faup_parse_domain, initFunc_faup_parse, NULL},
        {"faup_domain_without_tld", 1, 1, do_faup_parse_domain_without_tld, initFunc_faup_parse, NULL},
        {"faup_host", 1, 1, do_faup_parse_host, initFunc_faup_parse, NULL},
        {"faup_tld", 1, 1, do_faup_parse_tld, initFunc_faup_parse, NULL},
        {"faup_port", 1, 1, do_faup_parse_port, initFunc_faup_parse, NULL},
        {"faup_resource_path", 1, 1, do_faup_parse_resource_path, initFunc_faup_parse, NULL},
        {"faup_query_string", 1, 1, do_faup_parse_query_string, initFunc_faup_parse, NULL},
        {"faup_fragment", 1, 1, do_faup_parse_fragment, initFunc_faup_parse, NULL},
        {NULL, 0, 0, NULL, NULL, NULL} //last element to check end of array
};

BEGINgetFunctArray
CODESTARTgetFunctArray
    dbgprintf("Faup: ffaup\n");
    *version = 1;
    *functArray = functions;
ENDgetFunctArray


BEGINmodExit
CODESTARTmodExit
    dbgprintf("ffaup: freeing options\n");
    if(glbOptions){
        faup_options_free(glbOptions);
        glbOptions = NULL;
    }
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_FMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

    dbgprintf("ffaup: initializing options\n");
    glbOptions = faup_options_new();
    if(!glbOptions) {
        parser_errmsg("ffaup: could not initialize options\n");
        ABORT_FINALIZE(RS_RET_FAUP_INIT_OPTIONS_FAILED);
    }
    // Don't generate a string output, and don't load LUA modules
    // This is useful only for the faup executable
    glbOptions->output = FAUP_OUTPUT_NONE;
    glbOptions->exec_modules = FAUP_MODULES_NOEXEC;
CODEmodInit_QueryRegCFSLineHdlr
    dbgprintf("rsyslog ffaup init called, compiled with version %s\n", VERSION);
ENDmodInit
