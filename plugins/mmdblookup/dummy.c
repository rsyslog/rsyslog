/* a dummy module to be loaded if we cannot build this module, but
 * configure required it to be "optional".
 *
 * Copyright 2020 Rainer Gerhards and Adiscon GmbH.
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
#define MODULE_NAME "mmdblookup" /* how are we named? */

#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "parserif.h"


MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME(MODULE_NAME)


DEF_OMOD_STATIC_DATA;

/* config variables */
typedef struct _instanceData {
    char *dummy;
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData ATTR_UNUSED;
} wrkrInstanceData_t;

struct modConfData_s {};

/* modConf ptr to use for the current load process */
static modConfData_t *loadModConf = NULL;
/* modConf ptr to use for the current exec process */
static modConfData_t *runModConf = NULL;


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
ENDbeginCnfLoad

BEGINendCnfLoad
    CODESTARTendCnfLoad;
ENDendCnfLoad

BEGINcheckCnf
    CODESTARTcheckCnf;
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
    runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


BEGINsetModCnf
    CODESTARTsetModCnf;
    (void)lst;
    parser_errmsg(
        "%s is an optional module which could not be built on your platform "
        "please remove it from the configuration or upgrade your platform",
        MODULE_NAME);
ENDsetModCnf


BEGINnewActInst
    CODESTARTnewActInst;
    (void)pData;
    (void)ppModData;
    parser_errmsg(
        "%s is an optional module which could not be built on your platform "
        "please remove it from the configuration or upgrade your platform",
        MODULE_NAME);
ENDnewActInst


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
ENDdbgPrintInstInfo


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume


BEGINdoAction_NoStrings
    CODESTARTdoAction;
    (void)pMsgData;
ENDdoAction


NO_LEGACY_CONF_parseSelectorAct


    BEGINmodExit CODESTARTmodExit;
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    /* we only support the current interface specification */
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CODEmodInit_QueryRegCFSLineHdlr dbgprintf("mmdblookup: module compiled with rsyslog version %s.\n", VERSION);
ENDmodInit
