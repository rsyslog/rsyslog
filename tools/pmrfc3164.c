/* pmrfc3164.c
 * This is a parser module for RFC3164(legacy syslog)-formatted messages.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2009-11-04 by RGerhards
 *
 * Copyright 2007-2025 Rainer Gerhards and Adiscon GmbH.
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
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include "syslogd.h"
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "parser.h"
#include "datetime.h"
#include "unicode-helper.h"
#include "ruleset.h"
#include "rsconf.h"
#include "atomic.h"
MODULE_TYPE_PARSER
MODULE_TYPE_NOKEEP;
PARSER_NAME("rsyslog.rfc3164")
MODULE_CNFNAME("pmrfc3164")

/* internal structures
 */
DEF_PMOD_STATIC_DATA;
DEFobjCurrIf(glbl);
DEFobjCurrIf(parser);
DEFobjCurrIf(datetime);
DEFobjCurrIf(ruleset);


/* static data */
static int bParseHOSTNAMEandTAG; /* cache for the equally-named global param - performance enhancement */


/* parser instance parameters */
static struct cnfparamdescr parserpdescr[] = {
    {"detect.yearaftertimestamp", eCmdHdlrBinary, 0},
    {"permit.squarebracketsinhostname", eCmdHdlrBinary, 0},
    {"permit.slashesinhostname", eCmdHdlrBinary, 0},
    {"permit.atsignsinhostname", eCmdHdlrBinary, 0},
    {"force.tagendingbycolon", eCmdHdlrBinary, 0},
    {"remove.msgfirstspace", eCmdHdlrBinary, 0},
    {"detect.headerless", eCmdHdlrBinary, 0},
    {"headerless.hostname", eCmdHdlrString, 0},
    {"headerless.tag", eCmdHdlrString, 0},
    {"headerless.ruleset", eCmdHdlrString, 0},
    {"headerless.errorfile", eCmdHdlrString, 0},
    {"headerless.drop", eCmdHdlrBinary, 0},
};
static struct cnfparamblk parserpblk = {CNFPARAMBLK_VERSION, sizeof(parserpdescr) / sizeof(struct cnfparamdescr),
                                        parserpdescr};

struct instanceConf_s {
    int bDetectYearAfterTimestamp;
    int bPermitSquareBracketsInHostname;
    int bPermitSlashesInHostname;
    int bPermitAtSignsInHostname;
    int bForceTagEndingByColon;
    int bRemoveMsgFirstSpace;
    int bHdrLessMode; /** < is headerless mode activated? 0 - no, other - yes */
    uchar* pszHeaderlessHostname; /** < HOSTNAME to use for headerless messages */
    uchar* pszHeaderlessTag; /** < TAG to use for headerless messages */
    uchar* pszHeaderlessRulesetName; /** < name of Ruleset to use for headerless messages */
    ruleset_t* pHeaderlessRuleset; /**< Ruleset to use for headerless messages */
    DEF_ATOMIC_HELPER_MUT(mutHeaderlessRuleset); /**< mutex for atomic operations on pHeaderlessRuleset */
    uchar* pszHeaderlessErrFile; /**< name of error file for headerless messages */
    FILE* fpHeaderlessErr; /**< file pointer for error file (headerless) */
    pthread_mutex_t mutErrFile;
    int bDropHeaderless;
};


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATUREAutomaticSanitazion) iRet = RS_RET_OK;
    if (eFeat == sFEATUREAutomaticPRIParsing) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal createInstance(instanceConf_t** pinst) {
    instanceConf_t* inst;
    DEFiRet;
    CHKmalloc(inst = malloc(sizeof(instanceConf_t)));
    inst->bDetectYearAfterTimestamp = 0;
    inst->bPermitSquareBracketsInHostname = 0;
    inst->bPermitSlashesInHostname = 0;
    inst->bPermitAtSignsInHostname = 0;
    inst->bForceTagEndingByColon = 0;
    inst->bRemoveMsgFirstSpace = 0;
    inst->bHdrLessMode = 0;
    inst->pszHeaderlessHostname = NULL;
    inst->pszHeaderlessTag = NULL;
    inst->pszHeaderlessRulesetName = NULL;
    inst->pHeaderlessRuleset = NULL;
    INIT_ATOMIC_HELPER_MUT(inst->mutHeaderlessRuleset);
    inst->pszHeaderlessErrFile = NULL;
    inst->fpHeaderlessErr = NULL;
    pthread_mutex_init(&inst->mutErrFile, NULL);
    inst->bDropHeaderless = 0;
    bParseHOSTNAMEandTAG = glbl.GetParseHOSTNAMEandTAG(loadConf);
    *pinst = inst;
finalize_it:
    RETiRet;
}

BEGINnewParserInst
    struct cnfparamvals* pvals = NULL;
    int i;
    CODESTARTnewParserInst;
    DBGPRINTF("newParserInst (pmrfc3164)\n");

    inst = NULL;
    CHKiRet(createInstance(&inst));

    if (lst == NULL) FINALIZE; /* just set defaults, no param block! */

    if ((pvals = nvlstGetParams(lst, &parserpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("parser param blk in pmrfc3164:\n");
        cnfparamsPrint(&parserpblk, pvals);
    }

    for (i = 0; i < parserpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(parserpblk.descr[i].name, "detect.yearaftertimestamp")) {
            inst->bDetectYearAfterTimestamp = (int)pvals[i].val.d.n;
        } else if (!strcmp(parserpblk.descr[i].name, "permit.squarebracketsinhostname")) {
            inst->bPermitSquareBracketsInHostname = (int)pvals[i].val.d.n;
        } else if (!strcmp(parserpblk.descr[i].name, "permit.slashesinhostname")) {
            inst->bPermitSlashesInHostname = (int)pvals[i].val.d.n;
        } else if (!strcmp(parserpblk.descr[i].name, "permit.atsignsinhostname")) {
            inst->bPermitAtSignsInHostname = (int)pvals[i].val.d.n;
        } else if (!strcmp(parserpblk.descr[i].name, "force.tagendingbycolon")) {
            inst->bForceTagEndingByColon = (int)pvals[i].val.d.n;
        } else if (!strcmp(parserpblk.descr[i].name, "remove.msgfirstspace")) {
            inst->bRemoveMsgFirstSpace = (int)pvals[i].val.d.n;
        } else if (!strcmp(parserpblk.descr[i].name, "detect.headerless")) {
            inst->bHdrLessMode = (int)pvals[i].val.d.n;
        } else if (!strcmp(parserpblk.descr[i].name, "headerless.hostname")) {
            inst->pszHeaderlessHostname = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(parserpblk.descr[i].name, "headerless.tag")) {
            inst->pszHeaderlessTag = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(parserpblk.descr[i].name, "headerless.ruleset")) {
            inst->pszHeaderlessRulesetName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(parserpblk.descr[i].name, "headerless.errorfile")) {
            inst->pszHeaderlessErrFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(parserpblk.descr[i].name, "headerless.drop")) {
            inst->bDropHeaderless = (int)pvals[i].val.d.n;
        } else {
            dbgprintf(
                "pmrfc3164: program error, non-handled "
                "param '%s'\n",
                parserpblk.descr[i].name);
        }
    }
finalize_it:
    CODE_STD_FINALIZERnewParserInst if (lst != NULL) cnfparamvalsDestruct(pvals, &parserpblk);
    if (iRet != RS_RET_OK) free(inst);
ENDnewParserInst


BEGINfreeParserInst
    CODESTARTfreeParserInst;
    free(pInst->pszHeaderlessHostname);
    free(pInst->pszHeaderlessTag);
    free(pInst->pszHeaderlessRulesetName);
    free(pInst->pszHeaderlessErrFile);
    if (pInst->fpHeaderlessErr != NULL) fclose(pInst->fpHeaderlessErr);
    pthread_mutex_destroy(&pInst->mutErrFile);
    DESTROY_ATOMIC_HELPER_MUT(pInst->mutHeaderlessRuleset);
    dbgprintf("pmrfc3164: free parser instance %p\n", pInst);
ENDfreeParserInst

BEGINcheckParserInst
    CODESTARTcheckParserInst;

    if (pInst->pszHeaderlessRulesetName != NULL) {
        ruleset_t* myRuleset = NULL;
        rsRetVal localRet = ruleset.GetRuleset(loadConf, &myRuleset, pInst->pszHeaderlessRulesetName);
        if (localRet == RS_RET_OK && myRuleset != NULL) {
            pInst->pHeaderlessRuleset = myRuleset;
        } else {
            LogError(0, localRet,
                     "pmrfc3164: ruleset '%s' not found, cannot route headerless messages, "
                     "not assigning any ruleset to these messages.",
                     pInst->pszHeaderlessRulesetName);
        }
    }
ENDcheckParserInst

BEGINdoHUPParser
    CODESTARTdoHUPParser;
    instanceConf_t* pInst = (instanceConf_t*)pData;

    /* Close and reopen error file on HUP signal to support log rotation */
    if (pInst->pszHeaderlessErrFile != NULL) {
        pthread_mutex_lock(&pInst->mutErrFile);
        if (pInst->fpHeaderlessErr != NULL) {
            fclose(pInst->fpHeaderlessErr);
            pInst->fpHeaderlessErr = NULL;
            DBGPRINTF("pmrfc3164: closed error file %s on HUP\n", pInst->pszHeaderlessErrFile);
        }
        /* File will be reopened on next write attempt in handleHeaderlessMessage() */
        pthread_mutex_unlock(&pInst->mutErrFile);
    }
ENDdoHUPParser


/** Handle a headerless message by setting defaults and marking the object */
static ATTR_NONNULL() rsRetVal handleHeaderlessMessage(smsg_t* pMsg, instanceConf_t* const pInst, uchar* p2parse) {
    DEFiRet;

    uchar* hostname = pInst->pszHeaderlessHostname ? pInst->pszHeaderlessHostname : getRcvFrom(pMsg);
    if (hostname == NULL) hostname = (uchar*)"unknown";
    MsgSetHOSTNAME(pMsg, hostname, strlen((char*)hostname));

    uchar* tag = pInst->pszHeaderlessTag ? pInst->pszHeaderlessTag : (uchar*)"headerless";
    MsgSetTAG(pMsg, tag, strlen((char*)tag));

    MsgSetMSGoffs(pMsg, p2parse - pMsg->pszRawMsg);

    if (pInst->pHeaderlessRuleset != NULL) MsgSetRuleset(pMsg, pInst->pHeaderlessRuleset);

    if (pInst->pszHeaderlessErrFile != NULL) {
        pthread_mutex_lock(&pInst->mutErrFile);
        if (pInst->fpHeaderlessErr == NULL) {
            pInst->fpHeaderlessErr = fopen((char*)pInst->pszHeaderlessErrFile, "a");
            if (pInst->fpHeaderlessErr == NULL) {
                LogError(errno, RS_RET_ERR, "pmrfc3164: cannot open error file %s", pInst->pszHeaderlessErrFile);
            }
        }
        if (pInst->fpHeaderlessErr != NULL) {
            /* Write the raw message followed by a newline */
            if (fwrite(pMsg->pszRawMsg, 1, pMsg->iLenRawMsg, pInst->fpHeaderlessErr) != (size_t)pMsg->iLenRawMsg ||
                fputc('\n', pInst->fpHeaderlessErr) == EOF) {
                LogError(errno, RS_RET_IO_ERROR, "pmrfc3164: error writing to error file %s",
                         pInst->pszHeaderlessErrFile);
                fclose(pInst->fpHeaderlessErr);
                pInst->fpHeaderlessErr = NULL;
            }
        }
        pthread_mutex_unlock(&pInst->mutErrFile);
    }

    DBGPRINTF("pmrfc3164: headerless message handled with hostname='%s', tag='%s'\n", hostname, tag);

    if (pInst->bDropHeaderless) ABORT_FINALIZE(RS_RET_DISCARDMSG);

finalize_it:
    RETiRet;
}

/* parse a legay-formatted syslog message.
 * We apply heuristics during header detection. These are not 100% failure
 * prove, but the best compromise we came up within 20+ years of adapting
 * the heuristics.
 */
BEGINparse2
    uchar* p2parse;
    int lenMsg;
    int i; /* general index for parsing */
    uchar bufParseTAG[CONF_TAG_MAXSIZE];
    uchar bufParseHOSTNAME[CONF_HOSTNAME_MAXSIZE];
    CODESTARTparse;
    assert(pMsg != NULL);
    assert(pMsg->pszRawMsg != NULL);
    lenMsg = pMsg->iLenRawMsg - pMsg->offAfterPRI;
    DBGPRINTF("Message will now be parsed by the legacy syslog parser (offAfterPRI=%d, lenMsg=%d.\n", pMsg->offAfterPRI,
              lenMsg);
    /* note: offAfterPRI is already the number of PRI chars (do not add one!) */
    p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */
    setProtocolVersion(pMsg, MSG_LEGACY_PROTOCOL);
    if (pMsg->iFacility == (LOG_INVLD >> 3)) {
        DBGPRINTF("facility LOG_INVLD, do not parse\n");
        FINALIZE;
    }

    /* now check if we have a completely headerless message. This is always the
     * case if it starts with spaces or tabs followed '{' or '['.
     * Note that this is from the grown heuristics and not controlled by the
     * headerless options. // TODO: make transparent in conf?
     */
    i = 0;
    while (i < lenMsg && (p2parse[i] == ' ' || p2parse[i] == '\t')) {
        ++i;
    }
    if (i < lenMsg && (p2parse[i] == '{' || p2parse[i] == '[')) {
        DBGPRINTF("msg seems to be headerless, treating it as such\n");
        FINALIZE;
    }


    /* Check to see if msg contains a timestamp. We start by assuming
     * that the message timestamp is the time of reception (which we
     * generated ourselfs and then try to actually find one inside the
     * message. There we go from high-to low precison and are done
     * when we find a matching one. -- rgerhards, 2008-09-16
     */
    int bFoundTimestamp = 0; /**< indicates if we found a timestamp or not */
    if (datetime.ParseTIMESTAMP3339(&(pMsg->tTIMESTAMP), &p2parse, &lenMsg) == RS_RET_OK) {
        bFoundTimestamp = 1;
        /* we are done - parse pointer is moved by ParseTIMESTAMP3339 */;
    } else if (datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &p2parse, &lenMsg, NO_PARSE3164_TZSTRING,
                                           pInst->bDetectYearAfterTimestamp) == RS_RET_OK) {
        if (pMsg->dfltTZ[0] != '\0') applyDfltTZ(&pMsg->tTIMESTAMP, pMsg->dfltTZ);
        bFoundTimestamp = 1;
        /* we are done - parse pointer is moved by ParseTIMESTAMP3164 */;
    } else if (*p2parse == ' ' && lenMsg > 1) {
        /* try to see if it is slightly malformed - HP procurve seems to do that sometimes */
        ++p2parse; /* move over space */
        --lenMsg;
        if (datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &p2parse, &lenMsg, NO_PARSE3164_TZSTRING,
                                        pInst->bDetectYearAfterTimestamp) == RS_RET_OK) {
            /* indeed, we got it! */
            bFoundTimestamp = 1;
            /* we are done - parse pointer is moved by ParseTIMESTAMP3164 */;
        } else { /* parse pointer needs to be restored, as we moved it off-by-one
                  * for this try.
                  */
            --p2parse;
            ++lenMsg;
        }
    }

    if (pInst->bHdrLessMode != 0 && bFoundTimestamp == 0) {
        /* we do not have a valid timestamp in any of the formats we support
         * This is a strong indication that the message is headerless. So let's
         * process it according to headerless rules.
         */
        DBGPRINTF("msg seems to be headerless (no PRI, no timestamp), treating it as such\n");
        CHKiRet(handleHeaderlessMessage(pMsg, pInst, p2parse));
    }

    if (pMsg->msgFlags & IGNDATE) {
        /* we need to ignore the msg data, so simply copy over reception date */
        memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
    }

    /* rgerhards, 2006-03-13: next, we parse the hostname and tag. But we
     * do this only when the user has not forbidden this. I now introduce some
     * code that allows a user to configure rsyslogd to treat the rest of the
     * message as MSG part completely. In this case, the hostname will be the
     * machine that we received the message from and the tag will be empty. This
     * is meant to be an interim solution, but for now it is in the code.
     */
    if (bParseHOSTNAMEandTAG && !(pMsg->msgFlags & INTERNAL_MSG)) {
        /* parse HOSTNAME - but only if this is network-received!
         * rger, 2005-11-14: we still have a problem with BSD messages. These messages
         * do NOT include a host name. In most cases, this leads to the TAG to be treated
         * as hostname and the first word of the message as the TAG. Clearly, this is not
         * of advantage ;) I think I have now found a way to handle this situation: there
         * are certain characters which are frequently used in TAG (e.g. ':'), which are
         * *invalid* in host names. So while parsing the hostname, I check for these characters.
         * If I find them, I set a simple flag but continue. After parsing, I check the flag.
         * If it was set, then we most probably do not have a hostname but a TAG. Thus, I change
         * the fields. I think this logic shall work with any type of syslog message.
         * rgerhards, 2009-06-23: and I now have extended this logic to every character
         * that is not a valid hostname.
         * A "hostname" can validly include "[]" at the beginning and end. This sometimes
         * happens with IP address (e.g. "[192.168.0.1]"). This must be turned on via
         * an option as it may interfere with non-hostnames in some message formats.
         * rgerhards, 2015-04-20
         */
        if (lenMsg > 0 && pMsg->msgFlags & PARSE_HOSTNAME) {
            i = 0;
            int bHadSBracket = 0;
            if (pInst->bPermitSquareBracketsInHostname) {
                assert(i < lenMsg);
                if (p2parse[i] == '[') {
                    bHadSBracket = 1;
                    bufParseHOSTNAME[0] = '[';
                    ++i;
                }
            }
            while (i < lenMsg &&
                   (isalnum(p2parse[i]) || p2parse[i] == '.' || p2parse[i] == '_' || p2parse[i] == '-' ||
                    (p2parse[i] == ']' && bHadSBracket) || (p2parse[i] == '@' && pInst->bPermitAtSignsInHostname) ||
                    (p2parse[i] == '/' && pInst->bPermitSlashesInHostname)) &&
                   i < (CONF_HOSTNAME_MAXSIZE - 1)) {
                bufParseHOSTNAME[i] = p2parse[i];
                ++i;
                if (p2parse[i] == ']') break; /* must be closing bracket */
            }

            if (i == lenMsg) {
                /* we have a message that is empty immediately after the hostname,
                 * but the hostname thus is valid! -- rgerhards, 2010-02-22
                 */
                p2parse += i;
                lenMsg -= i;
                bufParseHOSTNAME[i] = '\0';
                MsgSetHOSTNAME(pMsg, bufParseHOSTNAME, i);
            } else {
                int isHostName = 0;
                if (i > 0) {
                    if (bHadSBracket) {
                        if (p2parse[i] == ']') {
                            bufParseHOSTNAME[i] = ']';
                            ++i;
                            isHostName = 1;
                        }
                    } else {
                        if (isalnum(p2parse[i - 1])) {
                            isHostName = 1;
                        }
                    }
                    if (p2parse[i] != ' ') isHostName = 0;
                }

                if (isHostName) {
                    /* we got a hostname! */
                    p2parse += i + 1; /* "eat" it (including SP delimiter) */
                    lenMsg -= i + 1;
                    bufParseHOSTNAME[i] = '\0';
                    MsgSetHOSTNAME(pMsg, bufParseHOSTNAME, i);
                }
            }
        }

        /* now parse TAG - that should be present in message from all sources.
         * This code is somewhat not compliant with RFC 3164. As of 3164,
         * the TAG field is ended by any non-alphanumeric character. In
         * practice, however, the TAG often contains dashes and other things,
         * which would end the TAG. So it is not desirable. As such, we only
         * accept colon and SP to be terminators. Even there is a slight difference:
         * a colon is PART of the TAG, while a SP is NOT part of the tag
         * (it is CONTENT). Starting 2008-04-04, we have removed the 32 character
         * size limit (from RFC3164) on the tag. This had bad effects on existing
         * environments, as sysklogd didn't obey it either (probably another bug
         * in RFC3164...). We now receive the full size, but will modify the
         * outputs so that only 32 characters max are used by default.
         */
        i = 0;
        while (lenMsg > 0 && *p2parse != ':' && *p2parse != ' ' && i < CONF_TAG_MAXSIZE - 2) {
            bufParseTAG[i++] = *p2parse++;
            --lenMsg;
        }
        if (lenMsg > 0 && *p2parse == ':') {
            ++p2parse;
            --lenMsg;
            bufParseTAG[i++] = ':';
        } else if (pInst->bForceTagEndingByColon) {
            /* Tag need to be ended by a colon or it's not a tag but the
             * begin of the message
             */
            p2parse -= (i + 1);
            lenMsg += (i + 1);
            i = 0;
            /* Default TAG is dash (without ':')
             */
            bufParseTAG[i++] = '-';
        }

        /* no TAG can only be detected if the message immediately ends, in which case an empty TAG
         * is considered OK. So we do not need to check for empty TAG. -- rgerhards, 2009-06-23
         */
        bufParseTAG[i] = '\0'; /* terminate string */
        MsgSetTAG(pMsg, bufParseTAG, i);
    } else { /* we enter this code area when the user has instructed rsyslog NOT
              * to parse HOSTNAME and TAG - rgerhards, 2006-03-13
              */
        if (!(pMsg->msgFlags & INTERNAL_MSG)) {
            DBGPRINTF("HOSTNAME and TAG not parsed by user configuration.\n");
        }
    }

finalize_it:
    if (pInst->bRemoveMsgFirstSpace && *p2parse == ' ') {
        /* Bypass first space found in MSG part */
        p2parse++;
        lenMsg--;
    }
    MsgSetMSGoffs(pMsg, p2parse - pMsg->pszRawMsg);
ENDparse2


BEGINmodExit
    CODESTARTmodExit;
    /* release what we no longer need */
    objRelease(glbl, CORE_COMPONENT);
    objRelease(parser, CORE_COMPONENT);
    objRelease(datetime, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_PMOD2_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
    CODEqueryEtryPt_doHUPParser;
ENDqueryEtryPt


BEGINmodInit(pmrfc3164)
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(parser, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));

    DBGPRINTF("rfc3164 parser init called\n");

    /* cache value, is set only during rsyslogd option processing */
    bParseHOSTNAMEandTAG = glbl.GetParseHOSTNAMEandTAG(loadConf);
ENDmodInit
