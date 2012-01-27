/* imklog.h
 * These are the definitions for the klog message generation module.
 *
 * File begun on 2007-12-17 by RGerhards
 * Major change: 2008-04-09: switched to a driver interface for 
 *     several platforms
 *
 * Copyright 2007-2012 Rainer Gerhards and Adiscon GmbH.
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
#ifndef	IMKLOG_H_INCLUDED
#define	IMKLOG_H_INCLUDED 1

#include "rsyslog.h"
#include "dirty.h"

/* interface to "drivers"
 * the platform specific drivers must implement these entry points. Only one
 * driver may be active at any given time, thus we simply rely on the linker
 * to resolve the addresses.
 * rgerhards, 2008-04-09
 */
rsRetVal klogLogKMsg(void);
rsRetVal klogWillRun(void);
rsRetVal klogAfterRun(void);
int klogFacilIntMsg(void);

/* the following data members may be accessed by the "drivers"
 * I admit this is not the cleanest way to doing things, but I honestly
 * believe it is appropriate for the job that needs to be done.
 * rgerhards, 2008-04-09
 */
extern int symbols_twice;
extern int use_syscall;
extern int symbol_lookup;
extern char *symfile; 
extern int console_log_level;
extern int dbgPrintSymbols;
extern uchar *pszPath;

/* the functions below may be called by the drivers */
rsRetVal imklogLogIntMsg(int priority, char *fmt, ...) __attribute__((format(printf,2, 3)));
rsRetVal Syslog(int priority, uchar *msg);

/* prototypes */
extern int klog_getMaxLine(void); /* work-around for klog drivers to get configured max line size */
extern int InitKsyms(char *);
extern void DeinitKsyms(void);
extern int InitMsyms(void);
extern void DeinitMsyms(void);
extern char * ExpandKadds(char *, char *);
extern void SetParanoiaLevel(int);

#endif /* #ifndef IMKLOG_H_INCLUDED */
/* vi:set ai:
 */
