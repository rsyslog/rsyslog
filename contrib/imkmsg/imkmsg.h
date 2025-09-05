/* imkmsg.h
 * These are the definitions for the kmsg message generation module.
 *
 * Copyright 2007-2023 Rainer Gerhards and Adiscon GmbH.
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
#ifndef IMKLOG_H_INCLUDED
#define IMKLOG_H_INCLUDED 1

#include "rsyslog.h"
#include "dirty.h"

typedef enum _kernel_ts_parse_mods {
    KMSG_PARSE_TS_OFF = 0,
    KMSG_PARSE_TS_ALWAYS = 1,
    KMSG_PARSE_TS_STARTUP_ONLY = 2
} t_kernel_ts_parse_mode;

typedef enum _kernel_readmode {
    KMSG_READMODE_FULL_BOOT = 0,
    KMSG_READMODE_FULL_ALWAYS = 1,
    KMSG_READMODE_NEW_ONLY = 2
} t_kernel_readmode;

/* we need to have the modConf type present in all submodules */
struct modConfData_s {
    rsconf_t *pConf;
    int iFacilIntMsg;
    uchar *pszPath;
    int console_log_level;
    int expected_boot_complete_secs;
    t_kernel_ts_parse_mode parseKernelStamp;
    t_kernel_readmode readMode;
    sbool configSetViaV2Method;
};

/* interface to "drivers"
 * the platform specific drivers must implement these entry points. Only one
 * driver may be active at any given time, thus we simply rely on the linker
 * to resolve the addresses.
 * rgerhards, 2008-04-09
 */
rsRetVal klogLogKMsg(modConfData_t *pModConf);
rsRetVal klogWillRunPrePrivDrop(modConfData_t *pModConf);
rsRetVal klogWillRunPostPrivDrop(modConfData_t *pModConf);
rsRetVal klogAfterRun(modConfData_t *pModConf);

/* the functions below may be called by the drivers */
rsRetVal imkmsgLogIntMsg(syslog_pri_t priority, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
rsRetVal Syslog(syslog_pri_t priority, uchar *msg, struct timeval *tp, struct json_object *json);
int klogFacilIntMsg(void);

/* prototypes */
extern int klog_getMaxLine(void); /* work-around for klog drivers to get configured max line size */
extern int InitKsyms(modConfData_t *);
extern void DeinitKsyms(void);
extern int InitMsyms(void);
extern void DeinitMsyms(void);
extern char *ExpandKadds(char *, char *);
extern void SetParanoiaLevel(int);

#endif /* #ifndef IMKLOG_H_INCLUDED */
