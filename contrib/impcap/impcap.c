/* impcap.c
 *
 * This is an input module using libpcap, a
 * portable C/C++ library for network traffic capture.
 * This module reads packets received from a network interface
 * using libpcap, to extract information such as IP addresses, ports,
 * protocols, etc... and make it available to rsyslog and other modules.
 *
 * File begun on 2018-11-13
 *
 * Created by:
 *  - Théo Bertin (theo.bertin@advens.fr)
 *
 * With:
 *  - François Bernard (francois.bernard@isen.yncrea.fr)
 *  - Tianyu Geng (tianyu.geng@isen.yncrea.fr)
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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <json.h>

#include <pcap.h>

#include "rsyslog.h"
#include "prop.h"
#include "ruleset.h"
#include "datetime.h"

#include "errmsg.h"
#include "unicode-helper.h"
#include "module-template.h"
#include "rainerscript.h"
#include "rsconf.h"
#include "glbl.h"
#include "srUtils.h"

#include "parsers.h"


MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("impcap")

#define DEFAULT_META_CONTAINER "!impcap"
#define DEFAULT_DATA_CONTAINER "!data"


/* static data */
DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(prop) DEFobjCurrIf(ruleset) DEFobjCurrIf(datetime)

    static prop_t *pInputName = NULL;

char *stringToHex(char *string, size_t length);

static ATTR_NORETURN void *startCaptureThread(void *instanceConf);

/* conf structures */

struct instanceConf_s {
    char *interface;
    uchar *filePath;
    pcap_t *device;
    uchar *filter;
    uchar *tag;
    uint8_t promiscuous;
    uint8_t immediateMode;
    uint32_t bufSize;
    uint8_t bufTimeout;
    uint8_t pktBatchCnt;
    pthread_t tid;
    uchar *pszBindRuleset; /* name of ruleset to bind to */
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    struct instanceConf_s *next;
};

struct modConfData_s {
    rsconf_t *pConf;
    instanceConf_t *root, *tail;
    uint16_t snap_length;
    uint8_t metadataOnly;
    char *metadataContainer;
    char *dataContainer;
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {{"interface", eCmdHdlrGetWord, 0},
                                           {"file", eCmdHdlrString, 0},
                                           {"promiscuous", eCmdHdlrBinary, 0},
                                           {"filter", eCmdHdlrString, 0},
                                           {"tag", eCmdHdlrString, 0},
                                           {"ruleset", eCmdHdlrString, 0},
                                           {"no_buffer", eCmdHdlrBinary, 0},
                                           {"buffer_size", eCmdHdlrPositiveInt, 0},
                                           {"buffer_timeout", eCmdHdlrPositiveInt, 0},
                                           {"packet_count", eCmdHdlrPositiveInt, 0}};
static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(struct cnfparamdescr), inppdescr};

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {{"snap_length", eCmdHdlrPositiveInt, 0},
                                           {"metadata_only", eCmdHdlrBinary, 0},
                                           {"metadata_container", eCmdHdlrGetWord, 0},
                                           {"data_container", eCmdHdlrGetWord, 0}};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

#include "im-helper.h"

/*
 * create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal createInstance(instanceConf_t **pinst) {
    instanceConf_t *inst;
    DEFiRet;
    CHKmalloc(inst = malloc(sizeof(instanceConf_t)));
    inst->next = NULL;
    inst->interface = NULL;
    inst->filePath = NULL;
    inst->device = NULL;
    inst->promiscuous = 0;
    inst->filter = NULL;
    inst->tag = NULL;
    inst->pszBindRuleset = NULL;
    inst->immediateMode = 0;
    inst->bufTimeout = 10;
    inst->bufSize = 1024 * 1024 * 15; /* should be enough for up to 10Gb interface*/
    inst->pktBatchCnt = 5;

    /* node created, let's add to global config */
    if (loadModConf->tail == NULL) {
        loadModConf->tail = loadModConf->root = inst;
    } else {
        loadModConf->tail->next = inst;
        loadModConf->tail = inst;
    }

    *pinst = inst;
finalize_it:
    RETiRet;
}

/* input instances */

BEGINnewInpInst
    struct cnfparamvals *pvals;
    instanceConf_t *inst;
    int i;
    CODESTARTnewInpInst;
    pvals = nvlstGetParams(lst, &inppblk, NULL);

    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "impcap: required parameters are missing\n");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&inst));

    for (i = 0; i < inppblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(inppblk.descr[i].name, "interface")) {
            inst->interface = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "file")) {
            inst->filePath = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "promiscuous")) {
            inst->promiscuous = (uint8_t)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "filter")) {
            inst->filter = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "tag")) {
            inst->tag = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            inst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "no_buffer")) {
            inst->immediateMode = (uint8_t)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "buffer_size")) {
            inst->bufSize = (uint32_t)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "buffer_timeout")) {
            inst->bufTimeout = (uint8_t)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "packet_count")) {
            inst->pktBatchCnt = (uint8_t)pvals[i].val.d.n;
        } else {
            dbgprintf("impcap: non-handled param %s in beginCnfLoad\n", inppblk.descr[i].name);
        }
    }

finalize_it:

    CODE_STD_FINALIZERnewInpInst cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst

/* global mod conf (v2 system) */
BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    int i;

    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "impcap: error processing module "
                 "config parameters missing [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "snap_length")) {
            loadModConf->snap_length = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "metadata_only")) {
            loadModConf->metadataOnly = (uint8_t)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "metadata_container")) {
            loadModConf->metadataContainer = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "data_container")) {
            loadModConf->dataContainer = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf("impcap: non-handled param %s in beginSetModCnf\n", modpblk.descr[i].name);
        }
    }

    if (!loadModConf->metadataContainer) CHKmalloc(loadModConf->metadataContainer = strdup(DEFAULT_META_CONTAINER));

    if (!loadModConf->dataContainer) CHKmalloc(loadModConf->dataContainer = strdup(DEFAULT_DATA_CONTAINER));
finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

/* config v2 system */

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    loadModConf->pConf = pConf;
    loadModConf->metadataOnly = 0;
    loadModConf->snap_length = 65535;
    loadModConf->metadataContainer = NULL;
    loadModConf->dataContainer = NULL;
ENDbeginCnfLoad

BEGINendCnfLoad
    CODESTARTendCnfLoad;
ENDendCnfLoad


/* function to generate error message if framework does not find requested ruleset */
static inline void std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst) {
    LogError(0, NO_ERRCODE,
             "impcap: ruleset '%s' for interface %s not found - "
             "using default ruleset instead",
             inst->pszBindRuleset, inst->interface);
}

BEGINcheckCnf
    instanceConf_t *inst;
    CODESTARTcheckCnf;
    if (pModConf->root == NULL) {
        LogError(0, RS_RET_NO_LISTNERS,
                 "impcap: module loaded, but "
                 "no interface defined - no input will be gathered");
        iRet = RS_RET_NO_LISTNERS;
    }

    if (pModConf->metadataOnly) { /* if metadata_only is "on", snap_length is overwritten */
        pModConf->snap_length = 100; /* arbitrary value, but should be enough for most protocols */
    }

    if (!pModConf->metadataContainer || !pModConf->dataContainer) {
        LogError(0, RS_RET_LOAD_ERROR,
                 "impcap: no name defined for metadata_container and "
                 "data_container, this shouldn't happen");
    } else {
        DBGPRINTF("impcap: metadata will be stored in '%s', and data in '%s'\n", pModConf->metadataContainer,
                  pModConf->dataContainer);
    }

    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        std_checkRuleset(pModConf, inst);
        if (inst->interface == NULL && inst->filePath == NULL) {
            iRet = RS_RET_INVALID_PARAMS;
            LogError(0, RS_RET_LOAD_ERROR, "impcap: 'interface' or 'file' must be specified");
            break;
        }
        if (inst->interface != NULL && inst->filePath != NULL) {
            iRet = RS_RET_INVALID_PARAMS;
            LogError(0, RS_RET_LOAD_ERROR, "impcap: either 'interface' or 'file' must be specified");
            break;
        }
    }

ENDcheckCnf

BEGINactivateCnfPrePrivDrop
    CODESTARTactivateCnfPrePrivDrop;
    runModConf = pModConf;
ENDactivateCnfPrePrivDrop

BEGINactivateCnf
    instanceConf_t *inst;
    pcap_t *dev = NULL;
    struct bpf_program filter_program;
    bpf_u_int32 SubNet, NetMask;
    char errBuf[PCAP_ERRBUF_SIZE];
    uint8_t retCode = 0;
    CODESTARTactivateCnf;
    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        if (inst->filePath != NULL) {
            dev = pcap_open_offline((const char *)inst->filePath, errBuf);
            if (dev == NULL) {
                LogError(0, RS_RET_LOAD_ERROR, "pcap: error while opening capture file: '%s'", errBuf);
                ABORT_FINALIZE(RS_RET_LOAD_ERROR);
            }
        } else if (inst->interface != NULL) {
            dev = pcap_create((const char *)inst->interface, errBuf);
            if (dev == NULL) {
                LogError(0, RS_RET_LOAD_ERROR, "pcap: error while creating packet capture: '%s'", errBuf);
                ABORT_FINALIZE(RS_RET_LOAD_ERROR);
            }

            DBGPRINTF("setting snap_length %d\n", pModConf->snap_length);
            if (pcap_set_snaplen(dev, pModConf->snap_length)) {
                LogError(0, RS_RET_LOAD_ERROR, "pcap: error while setting snap length: '%s'", pcap_geterr(dev));
                ABORT_FINALIZE(RS_RET_LOAD_ERROR);
            }

            DBGPRINTF("setting promiscuous %d\n", inst->promiscuous);
            if (pcap_set_promisc(dev, inst->promiscuous)) {
                LogError(0, RS_RET_LOAD_ERROR, "pcap: error while setting promiscuous mode: '%s'", pcap_geterr(dev));
                ABORT_FINALIZE(RS_RET_LOAD_ERROR);
            }

            if (inst->immediateMode) {
                DBGPRINTF("setting immediate mode %d\n", inst->immediateMode);
                retCode = pcap_set_immediate_mode(dev, inst->immediateMode);
                if (retCode) {
                    LogError(0, RS_RET_LOAD_ERROR,
                             "pcap: error while setting immediate mode: '%s',"
                             " using buffer instead\n",
                             pcap_geterr(dev));
                }
            }

            if (!inst->immediateMode || retCode) {
                DBGPRINTF("setting buffer size %u \n", inst->bufSize);
                if (pcap_set_buffer_size(dev, inst->bufSize)) {
                    LogError(0, RS_RET_LOAD_ERROR, "pcap: error while setting buffer size: '%s'", pcap_geterr(dev));
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                }
                DBGPRINTF("setting buffer timeout %dms\n", inst->bufTimeout);
                if (pcap_set_timeout(dev, inst->bufTimeout)) {
                    LogError(0, RS_RET_LOAD_ERROR, "pcap: error while setting buffer timeout: '%s'", pcap_geterr(dev));
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                }
            }

            switch (pcap_activate(dev)) {
                case PCAP_WARNING_PROMISC_NOTSUP:
                    LogError(0, NO_ERRCODE, "interface doesn't support promiscuous mode");
                    break;
                case PCAP_WARNING_TSTAMP_TYPE_NOTSUP:
                    LogError(0, NO_ERRCODE, "timestamp type is not supported");
                    break;
                case PCAP_WARNING:
                    LogError(0, NO_ERRCODE, "pcap: %s", pcap_geterr(dev));
                    break;
                case PCAP_ERROR_ACTIVATED:
                    LogError(0, RS_RET_LOAD_ERROR, "already activated, shouldn't happen");
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                case PCAP_ERROR_NO_SUCH_DEVICE:
                    LogError(0, RS_RET_LOAD_ERROR, "device doesn't exist");
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                case PCAP_ERROR_PERM_DENIED:
                    LogError(0, RS_RET_LOAD_ERROR,
                             "elevated privilege needed to open capture "
                             "interface");
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                case PCAP_ERROR_PROMISC_PERM_DENIED:
                    LogError(0, RS_RET_LOAD_ERROR,
                             "elevated privilege needed to put interface "
                             "in promiscuous mode");
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                case PCAP_ERROR_RFMON_NOTSUP:
                    LogError(0, RS_RET_LOAD_ERROR, "interface doesn't support monitor mode");
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                case PCAP_ERROR_IFACE_NOT_UP:
                    LogError(0, RS_RET_LOAD_ERROR, "interface is not up");
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                case PCAP_ERROR:
                    LogError(0, RS_RET_LOAD_ERROR, "pcap: %s", pcap_geterr(dev));
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                default:
                    // No action needed for other cases
                    break;
            }

            if (inst->filter != NULL) {
                DBGPRINTF("getting netmask on %s\n", inst->interface);
                // obtain the subnet
                if (pcap_lookupnet(inst->interface, &SubNet, &NetMask, errBuf)) {
                    DBGPRINTF("could not get netmask\n");
                    NetMask = PCAP_NETMASK_UNKNOWN;
                }
                DBGPRINTF("setting filter to '%s'\n", inst->filter);
                /* Compile the filter */
                if (pcap_compile(dev, &filter_program, (const char *)inst->filter, 1, NetMask)) {
                    LogError(0, RS_RET_LOAD_ERROR, "pcap: error while compiling filter: '%s'", pcap_geterr(dev));
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                } else if (pcap_setfilter(dev, &filter_program)) {
                    LogError(0, RS_RET_LOAD_ERROR, "pcap: error while setting filter: '%s'", pcap_geterr(dev));
                    pcap_freecode(&filter_program);
                    ABORT_FINALIZE(RS_RET_LOAD_ERROR);
                }
                pcap_freecode(&filter_program);
            }

            if (pcap_set_datalink(dev, DLT_EN10MB)) {
                LogError(0, RS_RET_LOAD_ERROR, "pcap: error while setting datalink type: '%s'", pcap_geterr(dev));
                ABORT_FINALIZE(RS_RET_LOAD_ERROR);
            }
        } /* inst->interface != NULL */
        else {
            LogError(0, RS_RET_LOAD_ERROR,
                     "impcap: no capture method specified, "
                     "please specify either 'interface' or 'file' in config");
            ABORT_FINALIZE(RS_RET_LOAD_ERROR);
        }

        inst->device = dev;
    }

finalize_it:
    if (iRet != 0) {
        if (dev) pcap_close(dev);
    }
ENDactivateCnf

BEGINfreeCnf
    instanceConf_t *inst, *del;
    CODESTARTfreeCnf;
    DBGPRINTF("impcap: freeing confs...\n");
    for (inst = pModConf->root; inst != NULL;) {
        del = inst;
        inst = inst->next;
        free(del->filePath);
        free(del->filter);
        free(del->pszBindRuleset);
        free(del->interface);
        free(del->tag);
        free(del);
    }
    free(pModConf->metadataContainer);
    free(pModConf->dataContainer);
    DBGPRINTF("impcap: finished freeing confs\n");
ENDfreeCnf

/* runtime functions */

/*
 *  Converts a list of bytes to their hexadecimal representation in ASCII
 *
 *  Gets the list of bytes and the length as parameters
 *
 *  Returns a pointer on the new list, being a string of ASCII characters
 *  representing hexadecimal values, in the form "A5B34C65..."
 *  its size is twice length parameter + 1
 */
char *stringToHex(char *string, size_t length) {
    const char *hexChar = "0123456789ABCDEF";
    char *retBuf;
    uint16_t i;

    retBuf = malloc((2 * length + 1) * sizeof(char));
    for (i = 0; i < length; ++i) {
        retBuf[2 * i] = hexChar[(string[i] & 0xF0) >> 4];
        retBuf[2 * i + 1] = hexChar[string[i] & 0x0F];
    }
    retBuf[2 * length] = '\0';

    return retBuf;
}

/*
 *  This method parses every packet received by libpcap, and is called by it
 *  It creates the message for Rsyslog, calls the parsers and add all necessary information
 *  in the message
 */
void packet_parse(uchar *arg, const struct pcap_pkthdr *pkthdr, const uchar *packet) {
    DBGPRINTF("impcap : entered packet_parse\n");
    smsg_t *pMsg;

    /* Prevent cast error from char to int with arg */
    union {
        uchar *buf;
        int *id;
    } aux;

    aux.buf = arg;
    int *id = aux.id;
    msgConstruct(&pMsg);

    MsgSetInputName(pMsg, pInputName);
    // search inst in loadmodconf,and check if there is tag. if so set tag in msg.
    pthread_t ctid = pthread_self();
    instanceConf_t *inst;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        if (pthread_equal(ctid, inst->tid)) {
            if (inst->pBindRuleset != NULL) {
                MsgSetRuleset(pMsg, inst->pBindRuleset);
            }
            if (inst->tag != NULL) {
                MsgSetTAG(pMsg, inst->tag, strlen((const char *)inst->tag));
            }
        }
    }


    struct json_object *jown = json_object_new_object();
    json_object_object_add(jown, "ID", json_object_new_int(++(*id)));

    struct syslogTime sysTimePkt;
    char timeStr[30];
    struct timeval tv = pkthdr->ts;
    datetime.timeval2syslogTime(&tv, &sysTimePkt, 1 /*inUTC*/);
    if (datetime.formatTimestamp3339(&sysTimePkt, timeStr)) {
        json_object_object_add(jown, "timestamp", json_object_new_string(timeStr));
    }

    json_object_object_add(jown, "net_bytes_total", json_object_new_int(pkthdr->len));

    data_ret_t *dataLeft = eth_parse(packet, pkthdr->caplen, jown);

    json_object_object_add(jown, "net_bytes_data", json_object_new_int(dataLeft->size));
    char *dataHex = stringToHex(dataLeft->pData, dataLeft->size);
    if (dataHex != NULL) {
        struct json_object *jadd = json_object_new_object();
        json_object_object_add(jadd, "length", json_object_new_int(strlen(dataHex)));
        json_object_object_add(jadd, "content", json_object_new_string(dataHex));
        msgAddJSON(pMsg, (uchar *)runModConf->dataContainer, jadd, 0, 0);
        free(dataHex);
    }
    free(dataLeft);

    msgAddJSON(pMsg, (uchar *)runModConf->metadataContainer, jown, 0, 0);
    submitMsg2(pMsg);
}

/* This is used to terminate the plugin.
 */
static void doSIGTTIN(int __attribute__((unused)) sig) {
    pthread_t tid = pthread_self();
    const int bTerminate = ATOMIC_FETCH_32BIT(&bTerminateInputs, &mutTerminateInputs);
    DBGPRINTF("impcap: awoken via SIGTTIN; bTerminateInputs: %d\n", bTerminate);
    if (bTerminate) {
        for (instanceConf_t *inst = runModConf->root; inst != NULL; inst = inst->next) {
            if (pthread_equal(tid, inst->tid)) {
                pcap_breakloop(inst->device);
                DBGPRINTF("impcap: thread %lx, termination requested via SIGTTIN - telling libpcap\n",
                          (long unsigned int)tid);
            }
        }
    }
}

/*
 *  This is the main function for each thread
 *  taking care of a specified network interface
 */
static ATTR_NORETURN void *startCaptureThread(void *instanceConf) {
    int id = 0;
    pthread_t tid = pthread_self();

    /* we want to support non-cancel input termination. To do so, we must signal libpcap
     * when to stop. As we run on the same thread, we need to register as SIGTTIN handler,
     * which will be used to put the terminating condition into libpcap.
     */
    DBGPRINTF("impcap: setting catch for SIGTTIN, thread %lx\n", (long unsigned int)tid);
    sigset_t sigSet;
    struct sigaction sigAct;
    sigfillset(&sigSet);
    pthread_sigmask(SIG_BLOCK, &sigSet, NULL);
    sigemptyset(&sigSet);
    sigaddset(&sigSet, SIGTTIN);
    pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);
    memset(&sigAct, 0, sizeof(sigAct));
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_handler = doSIGTTIN;
    sigaction(SIGTTIN, &sigAct, NULL);

    instanceConf_t *inst = (instanceConf_t *)instanceConf;
    DBGPRINTF("impcap: thread %lx, begin capture!\n", (long unsigned int)tid);
    while (glbl.GetGlobalInputTermState() == 0) {
        pcap_dispatch(inst->device, inst->pktBatchCnt, packet_parse, (uchar *)&id);
    }
    DBGPRINTF("impcap: thread %lx, capture finished\n", (long unsigned int)tid);
    pthread_exit(0);
}

BEGINrunInput
    instanceConf_t *inst;
    int ret = 0;
    CODESTARTrunInput;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        /* creates a thread and starts capturing on the interface */
        ret = pthread_create(&inst->tid, NULL, startCaptureThread, inst);
        if (ret) {
            LogError(0, RS_RET_NO_RUN, "impcap: error while creating threads\n");
        }
    }

    DBGPRINTF("impcap: starting to wait for close condition\n");
    // TODO: Use thread for capture instead of just waiting
    while (glbl.GetGlobalInputTermState() == 0) {
        if (glbl.GetGlobalInputTermState() == 0) srSleep(0, 400000);
    }

    DBGPRINTF("impcap: received close signal, signaling instance threads...\n");
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        pthread_kill(inst->tid, SIGTTIN);
    }

    DBGPRINTF("impcap: threads signaled, waiting for join...");
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        pthread_join(inst->tid, NULL);
        pcap_close(inst->device);
    }

    DBGPRINTF("impcap: finished threads, stopping\n");
ENDrunInput

BEGINwillRun
    CODESTARTwillRun;
    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("impcap"), sizeof("impcap") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
finalize_it:
ENDwillRun

BEGINafterRun
    CODESTARTafterRun;
    if (pInputName != NULL) {
        prop.Destruct(&pInputName);
    }
ENDafterRun

BEGINmodExit
    CODESTARTmodExit;
    DBGPRINTF("impcap:: modExit\n");
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
    objRelease(datetime, CORE_COMPONENT);
ENDmodExit

/* declaration of functions */

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURENonCancelInputTermination) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES /* might need it */
        CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));
ENDmodInit
