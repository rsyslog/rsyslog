/* The rsconf object. It models a complete rsyslog configuration.
 *
 * Copyright 2011-2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef INCLUDED_RSCONF_H
#define INCLUDED_RSCONF_H

#include "linkedlist.h"
#include "queue.h"
#include "lookup.h"
#include "dynstats.h"
#include "perctile_stats.h"
#include "timezones.h"
#include "ratelimit.h"

/* --- configuration objects (the plan is to have ALL upper layers in this file) --- */

#define REPORT_CHILD_PROCESS_EXITS_NONE 0
#define REPORT_CHILD_PROCESS_EXITS_ERRORS 1
#define REPORT_CHILD_PROCESS_EXITS_ALL 2

#ifndef DFLT_INT_MSGS_SEV_FILTER
    #define DFLT_INT_MSGS_SEV_FILTER 6 /* Warning level and more important */
#endif

struct queuecnf_s {
    int iMainMsgQueueSize; /* size of the main message queue above */
    int iMainMsgQHighWtrMark; /* high water mark for disk-assisted queues */
    int iMainMsgQLowWtrMark; /* low water mark for disk-assisted queues */
    int iMainMsgQDiscardMark; /* begin to discard messages */
    int iMainMsgQDiscardSeverity; /* by default, discard nothing to prevent unintentional loss */
    int iMainMsgQueueNumWorkers; /* number of worker threads for the mm queue above */
    queueType_t MainMsgQueType; /* type of the main message queue above */
    uchar *pszMainMsgQFName; /* prefix for the main message queue file */
    int64 iMainMsgQueMaxFileSize;
    int iMainMsgQPersistUpdCnt; /* persist queue info every n updates */
    int bMainMsgQSyncQeueFiles; /* sync queue files on every write? */
    int iMainMsgQtoQShutdown; /* queue shutdown (ms) */
    int iMainMsgQtoActShutdown; /* action shutdown (in phase 2) */
    int iMainMsgQtoEnq; /* timeout for queue enque */
    int iMainMsgQtoWrkShutdown; /* timeout for worker thread shutdown */
    int iMainMsgQWrkMinMsgs; /* minimum messages per worker needed to start a new one */
    int iMainMsgQDeqSlowdown; /* dequeue slowdown (simple rate limiting) */
    int64 iMainMsgQueMaxDiskSpace; /* max disk space allocated 0 ==> unlimited */
    int64 iMainMsgQueDeqBatchSize; /* dequeue batch size */
    int bMainMsgQSaveOnShutdown; /* save queue on shutdown (when DA enabled)? */
    int iMainMsgQueueDeqtWinFromHr; /* hour begin of time frame when queue is to be dequeued */
    int iMainMsgQueueDeqtWinToHr; /* hour begin of time frame when queue is to be dequeued */
};

/* parser config parameters */
struct parsercnf_s {
    uchar cCCEscapeChar; /* character to be used to start an escape sequence for control chars */
    int bDropTrailingLF; /* drop trailing LF's on reception? */
    int bEscapeCCOnRcv; /* escape control characters on reception: 0 - no, 1 - yes */
    int bSpaceLFOnRcv; /* replace newlines with spaces on reception: 0 - no, 1 - yes */
    int bEscape8BitChars; /* escape characters > 127 on reception: 0 - no, 1 - yes */
    int bEscapeTab; /* escape tab control character when doing CC escapes: 0 - no, 1 - yes */
    int bParserEscapeCCCStyle; /* escape control characters in c style: 0 - no, 1 - yes */
    int bPermitSlashInProgramname;
    int bParseHOSTNAMEandTAG; /* parser modification (based on startup params!) */
};

/* globals are data items that are really global, and can be set only
 * once (at least in theory, because the legacy system permits them to
 * be re-set as often as the user likes).
 */
struct globals_s {
#ifdef ENABLE_LIBCAPNG
    int bAbortOnFailedLibcapngSetup;
    int bCapabilityDropEnabled;
#endif
    int bDebugPrintTemplateList;
    int bDebugPrintModuleList;
    int bDebugPrintCfSysLineHandlerList;
    int bLogStatusMsgs; /* log rsyslog start/stop/HUP messages? */
    int bAllMsgToStderr; /* print all internal messages to stderr */
    int bErrMsgToStderr; /* print error messages to stderr
               (in addition to everything else)? */
    int maxErrMsgToStderr; /* how many messages to forward at most to stderr? */
    int bAbortOnUncleanConfig; /* abort run (rather than starting with partial
                      config) if there was any issue in conf */
    int bAbortOnFailedQueueStartup; /* similar to bAbortOnUncleanConfig, but abort if a queue
                       startup fails. This is not exactly an unclan config. */
    int uidDropPriv; /* user-id to which priveleges should be dropped to */
    int gidDropPriv; /* group-id to which priveleges should be dropped to */
    int gidDropPrivKeepSupplemental; /* keep supplemental groups when dropping? */
    int abortOnIDResolutionFail;
    int umask; /* umask to use */
    uchar *pszConfDAGFile; /* name of config DAG file, non-NULL means generate one */
    uchar *pszWorkDir;
    int bDropMalPTRMsgs; /* Drop messages which have malicious PTR records during DNS lookup */
    uchar *operatingStateFile;
    int debugOnShutdown; /* start debug log when we are shut down */
    int iGnuTLSLoglevel; /* Sets GNUTLS Debug Level */
    uchar *pszDfltNetstrmDrvrCAF; /* default CA file for the netstrm driver */
    uchar *pszDfltNetstrmDrvrCRLF; /* default CRL file for the netstrm driver */
    uchar *pszDfltNetstrmDrvrCertFile; /* default cert file for the netstrm driver (server) */
    uchar *pszDfltNetstrmDrvrKeyFile; /* default key file for the netstrm driver (server) */
    uchar *pszDfltNetstrmDrvr; /* module name of default netstream driver */
    uchar *pszNetstrmDrvrCAExtraFiles; /* CA extra file for the netstrm driver */
    uchar *pszDfltOpensslEngine; /* custom openssl engine */
    uchar *oversizeMsgErrorFile; /* File where oversize messages are written to */
    int reportOversizeMsg; /* shall error messages be generated for oversize messages? */
    int oversizeMsgInputMode; /* Mode which oversize messages will be forwarded */
    int reportChildProcessExits;
    int bActionReportSuspension;
    int bActionReportSuspensionCont;
    short janitorInterval; /* interval (in minutes) at which the janitor runs */
    int reportNewSenders;
    int reportGoneAwaySenders;
    int senderStatsTimeout;
    int senderKeepTrack; /* keep track of known senders? */
    int inputTimeoutShutdown; /* input shutdown timeout in ms */
    int iDefPFFamily; /* protocol family (IPv4, IPv6 or both) */
    int ACLAddHostnameOnFail; /* add hostname to acl when DNS resolving has failed */
    int ACLDontResolve; /* add hostname to acl instead of resolving it to IP(s) */
    int bDisableDNS; /* don't look up IP addresses of remote messages */
    int bProcessInternalMessages; /* Should rsyslog itself process internal messages?
                                   * 1 - yes
                                   * 0 - send them to libstdlog (e.g. to push to journal) or syslog()
                                   */
    uint64_t glblDevOptions; /* to be used by developers only */
    int intMsgRateLimitItv;
    int intMsgRateLimitBurst;
    int intMsgsSeverityFilter; /* filter for logging internal messages by syslog sev. */
    int permitCtlC;

    int actq_dflt_toQShutdown; /* queue shutdown */
    int actq_dflt_toActShutdown; /* action shutdown (in phase 2) */
    int actq_dflt_toEnq; /* timeout for queue enque */
    int actq_dflt_toWrkShutdown; /* timeout for worker thread shutdown */

    int ruleset_dflt_toQShutdown; /* queue shutdown */
    int ruleset_dflt_toActShutdown; /* action shutdown (in phase 2) */
    int ruleset_dflt_toEnq; /* timeout for queue enque */
    int ruleset_dflt_toWrkShutdown; /* timeout for worker thread shutdown */

    unsigned dnscacheDefaultTTL; /* 24 hrs default TTL */
    int dnscacheEnableTTL; /* expire entries or not (0) ? */
    int shutdownQueueDoubleSize;
    int optionDisallowWarning; /* complain if message from disallowed sender is received */
    int bSupportCompressionExtension;
#ifdef ENABLE_LIBLOGGING_STDLOG
    stdlog_channel_t stdlog_hdl; /* handle to be used for stdlog */
    uchar *stdlog_chanspec;
#endif
    int iMaxLine; /* maximum length of a syslog message */

    // TODO are the following ones defaults?
    int bReduceRepeatMsgs; /* reduce repeated message - 0 - no, 1 - yes */

    // TODO: other representation for main queue? Or just load it differently?
    queuecnf_t mainQ; /* main queue parameters */
    parsercnf_t parser; /* parser parameters */
};

/* (global) defaults are global in the sense that they are accessible
 * to all code, but they can change value and other objects (like
 * actions) actually copy the value a global had at the time the action
 * was defined. In that sense, a global default is just that, a default,
 * wich can (and will) be changed in the course of config file
 * processing. Once the config file has been processed, defaults
 * can be dropped. The current code does not do this for simplicity.
 * That is not a problem, because the defaults do not take up much memory.
 * At a later stage, we may think about dropping them. -- rgerhards, 2011-04-19
 */
struct defaults_s {
    int remove_me_when_first_real_member_is_added;
};


/* list of modules loaded in this configuration (config specific module list) */
struct cfgmodules_etry_s {
    cfgmodules_etry_t *next;
    modInfo_t *pMod;
    void *modCnf; /* pointer to the input module conf */
    /* the following data is input module specific */
    sbool canActivate; /* OK to activate this config? */
    sbool canRun; /* OK to run this config? */
};

struct cfgmodules_s {
    cfgmodules_etry_t *root;
};

/* outchannel-specific data */
struct outchannels_s {
    struct outchannel *ochRoot; /* the root of the outchannel list */
    struct outchannel *ochLast; /* points to the last element of the outchannel list */
};

struct templates_s {
    struct template *root; /* the root of the template list */
    struct template *last; /* points to the last element of the template list */
    struct template *lastStatic; /* last static element of the template list */
};

struct parsers_s {
    /* This is the list of all parsers known to us.
     * This is also used to unload all modules on shutdown.
     */
    parserList_t *pParsLstRoot;

    /* this is the list of the default parsers, to be used if no others
     * are specified.
     */
    parserList_t *pDfltParsLst;
};

struct actions_s {
    /* number of active actions */
    unsigned nbrActions;
    /* number of actions created. It is used to obtain unique IDs for the action. They
     * should not be relied on for any long-term activity (e.g. disk queue names!), but they are nice
     * to have during one instance of an rsyslogd run. For example, I use them to name actions when there
     * is no better name available.
     */
    int iActionNbr;
};


struct rulesets_s {
    linkedList_t llRulesets; /* this is NOT a pointer - no typo here ;) */

    /* support for legacy rsyslog.conf format */
    ruleset_t *pCurr; /* currently "active" ruleset */
    ruleset_t *pDflt; /* current default ruleset, e.g. for binding to actions which have no other */
};


/* --- end configuration objects --- */

/* the rsconf object */
struct rsconf_s {
    BEGINobjInstance
        ; /* Data to implement generic object - MUST be the first data element! */
        cfgmodules_t modules;
        globals_t globals;
        defaults_t defaults;
        templates_t templates;
        parsers_t parsers;
        lookup_tables_t lu_tabs;
        dynstats_buckets_t dynstats_buckets;
        perctile_buckets_t perctile_buckets;
        outchannels_t och;
        actions_t actions;
        rulesets_t rulesets;
        /* note: rulesets include the complete output part:
         *  - rules
         *  - filter (as part of the action)
         *  - actions
         * Of course, we need to debate if we shall change that some time...
         */
        timezones_t timezones;
        qqueue_t *pMsgQueue; /* the main message queue */
        ratelimit_cfgs_t ratelimit_cfgs;
};


/* interfaces */
BEGINinterface(rsconf) /* name must also be changed in ENDinterface macro! */
    INTERFACEObjDebugPrint(rsconf);
    rsRetVal (*Destruct)(rsconf_t **ppThis);
    rsRetVal (*Load)(rsconf_t **ppThis, uchar *confFile);
    rsRetVal (*Activate)(rsconf_t *ppThis);
ENDinterface(rsconf)
// TODO: switch version to 1 for first "complete" version!!!! 2011-04-20
#define rsconfCURR_IF_VERSION 0 /* increment whenever you change the interface above! */


/* prototypes */
PROTOTYPEObjFull(rsconf);
/* globally-visible external data */
extern rsconf_t *runConf; /* the currently running config */
extern rsconf_t *loadConf; /* the config currently being loaded (no concurrent config load supported!) */


int rsconfNeedDropPriv(rsconf_t *const cnf);

/* some defaults (to be removed?) */
#define DFLT_bLogStatusMsgs 1

#endif /* #ifndef INCLUDED_RSCONF_H */
