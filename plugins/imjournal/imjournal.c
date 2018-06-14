/* The systemd journal import module
 *
 * To test under Linux:
 * emmit log message into systemd journal
 *
 * Copyright (C) 2008-2017 Adiscon GmbH
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
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <errno.h>
#include <systemd/sd-journal.h>

#include "dirty.h"
#include "cfsysline.h"
#include "obj.h"
#include "msg.h"
#include "module-template.h"
#include "datetime.h"
#include "net.h"
#include "glbl.h"
#include "statsobj.h"
#include "parser.h"
#include "prop.h"
#include "errmsg.h"
#include "srUtils.h"
#include "unicode-helper.h"
#include "ratelimit.h"


MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imjournal")

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(datetime)
DEFobjCurrIf(glbl)
DEFobjCurrIf(parser)
DEFobjCurrIf(prop)
DEFobjCurrIf(net)
DEFobjCurrIf(statsobj)

struct modConfData_s {
	int bIgnPrevMsg;
};

static struct configSettings_s {
	char *stateFile;
	int iPersistStateInterval;
	int ratelimitInterval;
	int ratelimitBurst;
	int bIgnorePrevious;
	int bIgnoreNonValidStatefile;
	int iDfltSeverity;
	int iDfltFacility;
	int bUseJnlPID;
	char *usePid;
} cs;

static rsRetVal facilityHdlr(uchar **pp, void *pVal);

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "statefile", eCmdHdlrGetWord, 0 },
	{ "ratelimit.interval", eCmdHdlrInt, 0 },
	{ "ratelimit.burst", eCmdHdlrInt, 0 },
	{ "persiststateinterval", eCmdHdlrInt, 0 },
	{ "ignorepreviousmessages", eCmdHdlrBinary, 0 },
	{ "ignorenonvalidstatefile", eCmdHdlrBinary, 0 },
	{ "defaultseverity", eCmdHdlrSeverity, 0 },
	{ "defaultfacility", eCmdHdlrString, 0 },
	{ "usepidfromsystem", eCmdHdlrBinary, 0 },
	{ "usepid", eCmdHdlrString, 0 }
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

#define DFLT_persiststateinterval 10
#define DFLT_SEVERITY pri2sev(LOG_NOTICE)
#define DFLT_FACILITY pri2fac(LOG_USER)

static int bLegacyCnfModGlobalsPermitted = 1;/* are legacy module-global config parameters permitted? */

static prop_t *pInputName = NULL;
/* there is only one global inputName for all messages generated by this module */
static prop_t *pLocalHostIP = NULL;	/* a pseudo-constant propterty for 127.0.0.1 */
static const char *pidFieldName;	/* read-only after startup */
static int bPidFallBack;
static ratelimit_t *ratelimiter = NULL;
static sd_journal *j;
static int j_inotify_fd;
static struct {
	statsobj_t *stats;
	STATSCOUNTER_DEF(ctrSubmitted, mutCtrSubmitted)
	STATSCOUNTER_DEF(ctrRead, mutCtrRead);
	STATSCOUNTER_DEF(ctrDiscarded, mutCtrDiscarded);
	STATSCOUNTER_DEF(ctrFailed, mutCtrFailed);
	STATSCOUNTER_DEF(ctrPollFailed, mutCtrPollFailed);
	STATSCOUNTER_DEF(ctrRotations, mutCtrRotations);
	STATSCOUNTER_DEF(ctrRecoveryAttempts, mutCtrRecoveryAttempts);
	uint64 ratelimitDiscardedInInterval;
	uint64 diskUsageBytes;
} statsCounter;

#define J_PROCESS_PERIOD 1024  /* Call sd_journal_process() every 1,024 records */

static rsRetVal persistJournalState(void);
static rsRetVal loadJournalState(void);

static rsRetVal openJournal(void) {
	int r;
	DEFiRet;

	if ((r = sd_journal_open(&j, SD_JOURNAL_LOCAL_ONLY)) < 0) {
		LogError(-r, RS_RET_IO_ERROR, "imjournal: sd_journal_open() failed");
		iRet = RS_RET_IO_ERROR;
	}
	if ((r = sd_journal_get_fd(j)) < 0) {
		LogError(-r, RS_RET_IO_ERROR, "imjournal: sd_journal_get_fd() failed");
		iRet = RS_RET_IO_ERROR;
	} else {
		j_inotify_fd = r;
	}
	RETiRet;
}

static void closeJournal(void) {
	if (cs.stateFile) { /* can't persist without a state file */
		persistJournalState();
	}
	sd_journal_close(j);
	j_inotify_fd = 0;
}


/* ugly workaround to handle facility numbers; values
 * derived from names need to be eight times smaller,
 * i.e.: 0..23
 */
static rsRetVal facilityHdlr(uchar **pp, void *pVal)
{
	DEFiRet;
	char *p;

	skipWhiteSpace(pp);
	p = (char *) *pp;

	if (isdigit((int) *p)) {
		*((int *) pVal) = (int) strtol(p, (char **) pp, 10);
	} else {
		int len;
		syslogName_t *c;

		for (len = 0; p[len] && !isspace((int) p[len]); len++)
			/* noop */;
		for (c = syslogFacNames; c->c_name; c++) {
			if (!strncasecmp(p, (char *) c->c_name, len)) {
				*((int *) pVal) = pri2fac(c->c_val);
				break;
			}
		}
		*pp += len;
	}

	RETiRet;
}


/* Currently just replaces '\0' with ' '. Not doing so would cause
 * the value to be truncated. New space is allocated for the resulting
 * string.
 */
static rsRetVal
sanitizeValue(const char *in, size_t len, char **out)
{
	char *buf, *p;
	DEFiRet;

	CHKmalloc(p = buf = malloc(len + 1));
	memcpy(buf, in, len);
	buf[len] = '\0';

	while ((p = memchr(p, '\0', len + buf - p)) != NULL) {
		*p++ = ' ';
	}

	*out = buf;

finalize_it:
	RETiRet;
}


/* enqueue the the journal message into the message queue.
 * The provided msg string is not freed - thus must be done
 * by the caller.
 */
static rsRetVal
enqMsg(uchar *msg, uchar *pszTag, int iFacility, int iSeverity, struct timeval *tp, struct json_object *json,
int sharedJsonProperties)
{
	struct syslogTime st;
	smsg_t *pMsg;
	size_t len;
	DEFiRet;

	assert(msg != NULL);
	assert(pszTag != NULL);

	if(tp == NULL) {
		CHKiRet(msgConstruct(&pMsg));
	} else {
		datetime.timeval2syslogTime(tp, &st, TIME_IN_LOCALTIME);
		CHKiRet(msgConstructWithTime(&pMsg, &st, tp->tv_sec));
	}
	MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
	MsgSetInputName(pMsg, pInputName);
	len = strlen((char*)msg);
	MsgSetRawMsg(pMsg, (char*)msg, len);
	if(len > 0)
		parser.SanitizeMsg(pMsg);
	MsgSetMSGoffs(pMsg, 0);	/* we do not have a header... */
	MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
	MsgSetRcvFromIP(pMsg, pLocalHostIP);
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetTAG(pMsg, pszTag, ustrlen(pszTag));
	pMsg->iFacility = iFacility;
	pMsg->iSeverity = iSeverity;

	if(json != NULL) {
		msgAddJSON(pMsg, (uchar*)"!", json, 0, sharedJsonProperties);
	}

	CHKiRet(ratelimitAddMsg(ratelimiter, NULL, pMsg));
	STATSCOUNTER_INC(statsCounter.ctrSubmitted, statsCounter.mutCtrSubmitted);

finalize_it:
	if (iRet == RS_RET_DISCARDMSG)
		STATSCOUNTER_INC(statsCounter.ctrDiscarded, statsCounter.mutCtrDiscarded);

	RETiRet;
}


/* Read journal log while data are available, each read() reads one
 * record of printk buffer.
 */
static rsRetVal
readjournal(void)
{
	DEFiRet;

	struct timeval tv;
	uint64_t timestamp;

	struct json_object *json = NULL;
	int r;

	/* Information from messages */
	char *message = NULL;
	char *sys_iden;
	char *sys_iden_help = NULL;

	const void *get;
	const void *pidget;
	size_t length;
	size_t pidlength;

	const void *equal_sign;
	struct json_object *jval;
	size_t l;

	long prefixlen = 0;

	int severity = cs.iDfltSeverity;
	int facility = cs.iDfltFacility;

	/* Get message text */
	if (sd_journal_get_data(j, "MESSAGE", &get, &length) < 0) {
		message = strdup("");
	} else {
		CHKiRet(sanitizeValue(((const char *)get) + 8, length - 8, &message));
	}
	STATSCOUNTER_INC(statsCounter.ctrRead, statsCouner.mutCtrRead);

	/* Get message severity ("priority" in journald's terminology) */
	if (sd_journal_get_data(j, "PRIORITY", &get, &length) >= 0) {
		if (length == 10) {
			severity = ((char *)get)[9] - '0';
			if (severity < 0 || 7 < severity) {
				LogError(0, RS_RET_ERR, "imjournal: the value of the 'PRIORITY' field is "
					"out of bounds: %d, resetting", severity);
				severity = cs.iDfltSeverity;
			}
		} else {
			LogError(0, RS_RET_ERR, "The value of the 'PRIORITY' field has an "
				"unexpected length: %zu\n", length);
		}
	}

	/* Get syslog facility */
	if (sd_journal_get_data(j, "SYSLOG_FACILITY", &get, &length) >= 0) {
		// Note: the journal frequently contains invalid facilities!
		if (length == 17 || length == 18) {
			facility = ((char *)get)[16] - '0';
			if (length == 18) {
				facility *= 10;
				facility += ((char *)get)[17] - '0';
			}
			if (facility < 0 || 23 < facility) {
				DBGPRINTF("The value of the 'FACILITY' field is "
					"out of bounds: %d, resetting\n", facility);
				facility = cs.iDfltFacility;
			}
		} else {
			DBGPRINTF("The value of the 'FACILITY' field has an "
				"unexpected length: %zu value: '%s'\n", length, (const char*)get);
		}
	}

	/* Get message identifier, client pid and add ':' */
	if (sd_journal_get_data(j, "SYSLOG_IDENTIFIER", &get, &length) >= 0) {
		CHKiRet(sanitizeValue(((const char *)get) + 18, length - 18, &sys_iden));
	} else {
		CHKmalloc(sys_iden = strdup("journal"));
	}

	/* trying to get PID, default is "SYSLOG_PID" property */
	if (sd_journal_get_data(j, pidFieldName, &pidget, &pidlength) >= 0) {
		char *sys_pid;
		int val_ofs;

		val_ofs = strlen(pidFieldName) + 1; /* name + '=' */
		CHKiRet_Hdlr(sanitizeValue(((const char *)pidget) + val_ofs, pidlength - val_ofs, &sys_pid)) {
			free (sys_iden);
			FINALIZE;
		}
		r = asprintf(&sys_iden_help, "%s[%s]:", sys_iden, sys_pid);
		free (sys_pid);
	} else {
		/* this is fallback, "SYSLOG_PID" doesn't exist so trying to get "_PID" property */
		if (bPidFallBack && sd_journal_get_data(j, "_PID", &pidget, &pidlength) >= 0) {
			char *sys_pid;
			int val_ofs;

			val_ofs = strlen("_PID") + 1; /* name + '=' */
			CHKiRet_Hdlr(sanitizeValue(((const char *)pidget) + val_ofs, pidlength - val_ofs, &sys_pid)) {
				free (sys_iden);
				FINALIZE;
			}
			r = asprintf(&sys_iden_help, "%s[%s]:", sys_iden, sys_pid);
			free (sys_pid);
		} else {
			/* there is no PID property available */
			r = asprintf(&sys_iden_help, "%s:", sys_iden);
		}
	}

	free (sys_iden);

	if (-1 == r) {
		STATSCOUNTER_INC(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	json = json_object_new_object();

	SD_JOURNAL_FOREACH_DATA(j, get, l) {
		char *data;
		char *name;

		/* locate equal sign, this is always present */
		equal_sign = memchr(get, '=', l);

		/* ... but we know better than to trust the specs */
		if (equal_sign == NULL) {
			LogError(0, RS_RET_ERR, "SD_JOURNAL_FOREACH_DATA()"
				"returned a malformed field (has no '='): '%s'", (char*)get);
			continue; /* skip the entry */
		}

		/* get length of journal data prefix */
		prefixlen = ((char *)equal_sign - (char *)get);

		name = strndup(get, prefixlen);
		CHKmalloc(name);

		prefixlen++; /* remove '=' */

		CHKiRet_Hdlr(sanitizeValue(((const char *)get) + prefixlen, l - prefixlen, &data)) {
			free (name);
			FINALIZE;
		}

		/* and save them to json object */
		jval = json_object_new_string((char *)data);
		json_object_object_add(json, name, jval);
		free (data);
		free (name);
	}

	/* calculate timestamp */
	if (sd_journal_get_realtime_usec(j, &timestamp) >= 0) {
		tv.tv_sec = timestamp / 1000000;
		tv.tv_usec = timestamp % 1000000;
	}

	/* submit message */
	enqMsg((uchar *)message, (uchar *) sys_iden_help, facility, severity, &tv, json, 0);

finalize_it:
	free(sys_iden_help);
	free(message);
	RETiRet;
}


/* This function gets journal cursor and saves it into state file
 */
static rsRetVal
persistJournalState(void)
{
	DEFiRet;
	FILE *sf; /* state file */
	char tmp_sf[MAXFNAME];
	char *cursor;
	int ret = 0;

	/* On success, sd_journal_get_cursor() returns 1 in systemd
	   197 or older and 0 in systemd 198 or newer */
	if ((ret = sd_journal_get_cursor(j, &cursor)) >= 0) {
               /* we create a temporary name by adding a ".tmp"
                * suffix to the end of our state file's name
                */
               snprintf(tmp_sf, sizeof(tmp_sf), "%s.tmp", cs.stateFile);
               if ((sf = fopen(tmp_sf, "wb")) != NULL) {
			if (fprintf(sf, "%s", cursor) < 0) {
				iRet = RS_RET_IO_ERROR;
			}
			fclose(sf);
			free(cursor);
                       /* change the name of the file to the configured one */
                       if (iRet == RS_RET_OK && rename(tmp_sf, cs.stateFile) == -1) {
                               LogError(errno, iRet, "imjournal: rename() failed: "
                                       "for new path: '%s'", cs.stateFile);
                               iRet = RS_RET_IO_ERROR;
                       }

		} else {
			LogError(errno, RS_RET_FOPEN_FAILURE, "imjournal: fopen() failed "
				"for path: '%s'", tmp_sf);
			iRet = RS_RET_FOPEN_FAILURE;
		}
	} else {
		LogError(-ret, RS_RET_ERR, "imjournal: sd_journal_get_cursor() failed");
		iRet = RS_RET_ERR;
	}
	RETiRet;
}


static rsRetVal skipOldMessages(void);
/* Polls the journal for new messages. Similar to sd_journal_wait()
 * except for the special handling of EINTR.
 */

#define POLL_TIMEOUT 1000 /* timeout for poll is 1s */

static rsRetVal
pollJournal(void)
{
	DEFiRet;
	struct pollfd pollfd;
	int err; // journal error code to process
	int pr = 0;

	pollfd.fd = j_inotify_fd;
	pollfd.events = sd_journal_get_events(j);
#ifdef NEW_JOURNAL
	pr = poll(&pollfd, 1, POLL_TIMEOUT);
#else
	pr = poll(&pollfd, 1, -1);
#endif
	if (pr == -1) {
		if (errno == EINTR) {
			/* EINTR is also received during termination
			 * so return now to check the term state.
			 */
			ABORT_FINALIZE(RS_RET_OK);
		} else {
			LogError(errno, RS_RET_ERR, "imjournal: poll() failed");
			STATSCOUNTER_INC(statsCounter.ctrPollFailed, statsCounter.mutCtrPollFailed)
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}
#ifndef NEW_JOURNAL
	assert(pr == 1);
#endif

	err = sd_journal_process(j);
	if (err < 0) {
		LogError(-err, RS_RET_ERR, "imjournal: sd_journal_process() failed");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if (err == SD_JOURNAL_INVALIDATE)
		STATSCOUNTER_INC(statsCounter.ctrRotations, statsCounter.mutCtrRotations);

finalize_it:
	RETiRet;
}


static rsRetVal
skipOldMessages(void)
{
	int r;
	DEFiRet;

	if ((r = sd_journal_seek_tail(j)) < 0) {
		LogError(-r, RS_RET_ERR,
			"imjournal: sd_journal_seek_tail() failed");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if ((r = sd_journal_previous(j)) < 0) {
		LogError(-r, RS_RET_ERR,
			"imjournal: sd_journal_previous() failed");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	RETiRet;
}

/* This function loads a journal cursor from the state file.
 */
static rsRetVal
loadJournalState(void)
{
	DEFiRet;
	int r;
	FILE *r_sf;

	if (cs.stateFile[0] != '/') {
		char *new_stateFile;
		if (-1 == asprintf(&new_stateFile, "%s/%s", (char *)glbl.GetWorkDir(), cs.stateFile)) {
			LogError(0, RS_RET_OUT_OF_MEMORY, "imjournal: asprintf failed\n");
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		free (cs.stateFile);
		cs.stateFile = new_stateFile;
	}

	if ((r_sf = fopen(cs.stateFile, "rb")) != NULL) {
		char readCursor[128 + 1];
		if (fscanf(r_sf, "%128s\n", readCursor) != EOF) {
			if (sd_journal_seek_cursor(j, readCursor) != 0) {
				LogError(0, RS_RET_ERR, "imjournal: "
					"couldn't seek to cursor `%s'\n", readCursor);
				iRet = RS_RET_ERR;
			} else {
				char * tmp_cursor = NULL;
				sd_journal_next(j);
				/*
				* This is resolving the situation when system is after reboot and boot_id
				* doesn't match so cursor pointing into "future".
				* Usually sd_journal_next jump to head of journal due to journal aproximation,
				* but when system time goes backwards and cursor is still 
				  invalid, rsyslog stops logging.
				* We use sd_journal_get_cursor to validate our cursor.
				* When cursor is invalid we are trying to jump to the head of journal
				* This problem with time should not affect persistent journal,
				* but if cursor has been intentionally compromised it could stop logging even
				* with persistent journal.
				* */
				if ((r = sd_journal_get_cursor(j, &tmp_cursor)) < 0) {
					LogError(-r, RS_RET_IO_ERROR, "imjournal: "
					"loaded invalid cursor, seeking to the head of journal\n");
					if ((r = sd_journal_seek_head(j)) < 0) {
						LogError(-r, RS_RET_ERR, "imjournal: "
						"sd_journal_seek_head() failed, when cursor is invalid\n");
						iRet = RS_RET_ERR;
					}
				}
			}
		} else {
			LogError(0, RS_RET_IO_ERROR, "imjournal: "
				"fscanf on state file `%s' failed\n", cs.stateFile);
			iRet = RS_RET_IO_ERROR;
		}

		fclose(r_sf);

		if (iRet != RS_RET_OK && cs.bIgnoreNonValidStatefile) {
			/* ignore state file errors */
			iRet = RS_RET_OK;
			LogError(0, NO_ERRCODE, "imjournal: ignoring invalid state file %s",
				cs.stateFile);
			if (cs.bIgnorePrevious) {
				skipOldMessages();
			}
		}
	} else {
		LogError(0, RS_RET_FOPEN_FAILURE, "imjournal: "
				"open on state file `%s' failed\n", cs.stateFile);
		if (cs.bIgnorePrevious) {
			/* Seek to the very end of the journal and ignore all
			 * older messages. */
			skipOldMessages();
		}
	}

finalize_it:
	RETiRet;
}

static void
tryRecover(void) {
	LogMsg(0, RS_RET_OK, LOG_INFO, "imjournal: trying to recover from unexpected "
		"journal error");
	STATSCOUNTER_INC(statsCounter.ctrRecoveryAttempts, statsCounter.mutCtrRecoveryAttempts);
	closeJournal();
	srSleep(10, 0);	// do not hammer machine with too-frequent retries
	openJournal();
}


BEGINrunInput
	uint64_t count = 0;
CODESTARTrunInput
	CHKiRet(ratelimitNew(&ratelimiter, "imjournal", NULL));
	dbgprintf("imjournal: ratelimiting burst %d, interval %d\n", cs.ratelimitBurst,
		  cs.ratelimitInterval);
	ratelimitSetLinuxLike(ratelimiter, cs.ratelimitInterval, cs.ratelimitBurst);
	ratelimitSetNoTimeCache(ratelimiter);

	if (cs.stateFile) {
		/* Load our position in the journal from the state file. */
		CHKiRet(loadJournalState());
	} else if (cs.bIgnorePrevious) {
		/* Seek to the very end of the journal and ignore all
		 * older messages. */
		skipOldMessages();
	}

	/* handling old "usepidfromsystem" option */
	if (cs.bUseJnlPID != -1) {
		free(cs.usePid);
		cs.usePid = strdup("system");
		LogError(0, RS_RET_DEPRECATED,
			"\"usepidfromsystem\" is depricated, use \"usepid\" instead");
	}

	if (cs.usePid && (strcmp(cs.usePid, "system") == 0)) {
		pidFieldName = "_PID";
		bPidFallBack = 0;
	} else if (cs.usePid && (strcmp(cs.usePid, "syslog") == 0)) {
		pidFieldName = "SYSLOG_PID";
		bPidFallBack = 0;
	} else  {
		pidFieldName = "SYSLOG_PID";
		bPidFallBack = 1;
		if (cs.usePid && (strcmp(cs.usePid, "both") != 0)) {
			LogError(0, RS_RET_OK, "option \"usepid\""
				" should contain one of system|syslog|both and no '%s'",cs.usePid);
		}
	}


	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework.
	 */
	while (glbl.GetGlobalInputTermState() == 0) {
		int r;

		r = sd_journal_next(j);
		if (r < 0) {
			LogError(-r, RS_RET_ERR, "imjournal: sd_journal_next() failed");
			tryRecover();
			continue;
		}

		if (r == 0) {
			/* No new messages, wait for activity. */
			if (pollJournal() != RS_RET_OK) {
				tryRecover();
			}
			continue;
		}

		/*
		 * update journal disk usage before reading the new message.
		 */
		if (sd_journal_get_usage(j, (uint64_t *)&statsCounter.diskUsageBytes) < 0) {
			LogError(0, RS_RET_ERR, "imjournal: sd_get_usage() failed");
		}

		if (readjournal() != RS_RET_OK) {
			tryRecover();
			continue;
		}

		count++;

		if ((count % J_PROCESS_PERIOD) == 0) {
			/* Give the journal a periodic chance to detect rotated journal files to be cleaned up. */
			r = sd_journal_process(j);
			if (r < 0) {
				LogError(-r, RS_RET_ERR, "imjournal: sd_journal_process() failed");
				tryRecover();
				continue;
			}
		}

		if (cs.stateFile) { /* can't persist without a state file */
			/* TODO: This could use some finer metric. */
			if ((count % cs.iPersistStateInterval) == 0) {
				persistJournalState();
			}
		}
	}

finalize_it:
ENDrunInput


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	bLegacyCnfModGlobalsPermitted = 1;

	cs.bIgnoreNonValidStatefile = 1;
	cs.iPersistStateInterval = DFLT_persiststateinterval;
	cs.stateFile = NULL;
	cs.ratelimitBurst = 20000;
	cs.ratelimitInterval = 600;
	cs.iDfltSeverity = DFLT_SEVERITY;
	cs.iDfltFacility = DFLT_FACILITY;
	cs.bUseJnlPID = -1;
	cs.usePid = NULL;
ENDbeginCnfLoad


BEGINendCnfLoad
CODESTARTendCnfLoad
ENDendCnfLoad


BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf


BEGINactivateCnf
CODESTARTactivateCnf

	/* support statistic gathering */
	CHKiRet(statsobj.Construct(&(statsCounter.stats)));
	CHKiRet(statsobj.SetName(statsCounter.stats, (uchar*)"imjournal"));
	CHKiRet(statsobj.SetOrigin(statsCounter.stats, (uchar*)"imjournal"));
	STATSCOUNTER_INIT(statsCounter.ctrSubmitted, statsCounter.mutCtrSubmitted);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("submitted"),
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrSubmitted)));
	STATSCOUNTER_INIT(statsCounter.ctrRead, statsCounter.mutCtrRead);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("read"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrRead)));
	STATSCOUNTER_INIT(statsCounter.ctrDiscarded, statsCounter.mutCtrDiscarded);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("discarded"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrDiscarded)));
	STATSCOUNTER_INIT(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("failed"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrFailed)));
	STATSCOUNTER_INIT(statsCounter.ctrPollFailed, statsCounter.mutCtrPollFailed);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("poll_failed"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrPollFailed)));
	STATSCOUNTER_INIT(statsCounter.ctrRotations, statsCounter.mutCtrRotations);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("rotations"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrRotations)));
	STATSCOUNTER_INIT(statsCounter.ctrRecoveryAttempts, statsCounter.mutCtrRecoveryAttempts);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("recovery_attempts"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrRecoveryAttempts)));
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("ratelimit_discarded_in_interval"),
	                ctrType_Int, CTR_FLAG_NONE, &(statsCounter.ratelimitDiscardedInInterval)));
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("disk_usage_bytes"),
	                ctrType_Int, CTR_FLAG_NONE, &(statsCounter.diskUsageBytes)));
	CHKiRet(statsobj.ConstructFinalize(statsCounter.stats));
	/* end stats counter */

finalize_it:
ENDactivateCnf


BEGINfreeCnf
CODESTARTfreeCnf
	free(cs.stateFile);
	free(cs.usePid);
	statsobj.Destruct(&(statsCounter.stats));
ENDfreeCnf

/* open journal */
BEGINwillRun
CODESTARTwillRun
	iRet = openJournal();
ENDwillRun

/* close journal */
BEGINafterRun
CODESTARTafterRun
	closeJournal();
	ratelimitDestruct(ratelimiter);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	if(pInputName != NULL)
		prop.Destruct(&pInputName);
	if(pLocalHostIP != NULL)
		prop.Destruct(&pLocalHostIP);

	/* release objects we used */
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(net, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
ENDmodExit


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if (pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if (Debug) {
		dbgprintf("module (global) param blk for imjournal:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for (i = 0 ; i < modpblk.nParams ; ++i) {
		if (!pvals[i].bUsed)
			continue;
		if (!strcmp(modpblk.descr[i].name, "persiststateinterval")) {
			cs.iPersistStateInterval = (int) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, "statefile")) {
			cs.stateFile = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "ratelimit.burst")) {
			cs.ratelimitBurst = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "ratelimit.interval")) {
			cs.ratelimitInterval = (int) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, "ignorepreviousmessages")) {
			cs.bIgnorePrevious = (int) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, "ignorenonvalidstatefile")) {
			cs.bIgnoreNonValidStatefile = (int) pvals[i].val.d.n; 
		} else if (!strcmp(modpblk.descr[i].name, "defaultseverity")) {
			cs.iDfltSeverity = (int) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, "defaultfacility")) {
			/* ugly workaround to handle facility numbers; values
			   derived from names need to be eight times smaller */

			char *fac, *p;

			fac = p = es_str2cstr(pvals[i].val.d.estr, NULL);
			facilityHdlr((uchar **) &p, (void *) &cs.iDfltFacility);
			free(fac);
		} else if (!strcmp(modpblk.descr[i].name, "usepidfromsystem")) {
			cs.bUseJnlPID = (int) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, "usepid")) {
			cs.usePid = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("imjournal: program error, non-handled "
				"param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}


finalize_it:
	if (pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(net, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.CreateStringProp(&pInputName, UCHAR_CONSTANT("imjournal"), sizeof("imjournal") - 1));
	CHKiRet(prop.CreateStringProp(&pLocalHostIP, UCHAR_CONSTANT("127.0.0.1"), sizeof("127.0.0.1") - 1));

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournalpersiststateinterval", 0, eCmdHdlrInt,
		NULL, &cs.iPersistStateInterval, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournalratelimitinterval", 0, eCmdHdlrInt,
		NULL, &cs.ratelimitInterval, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournalratelimitburst", 0, eCmdHdlrInt,
		NULL, &cs.ratelimitBurst, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournalstatefile", 0, eCmdHdlrGetWord,
		NULL, &cs.stateFile, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournalignorepreviousmessages", 0, eCmdHdlrBinary,
		NULL, &cs.bIgnorePrevious, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournaldefaultseverity", 0, eCmdHdlrSeverity,
		NULL, &cs.iDfltSeverity, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournaldefaultfacility", 0, eCmdHdlrCustomHandler,
		facilityHdlr, &cs.iDfltFacility, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imjournalusepidfromsystem", 0, eCmdHdlrBinary,
		NULL, &cs.bUseJnlPID, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
