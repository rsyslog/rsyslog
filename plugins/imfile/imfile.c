/* imfile.c
 *
 * This is the input module for reading text file data. A text file is a
 * non-binary file who's lines are delemited by the \n character.
 *
 * Work originally begun on 2008-02-01 by Rainer Gerhards
 *
 * Copyright 2008-2016 Adiscon GmbH.
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>		/* do NOT remove: will soon be done by the module generation macros */
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>
#include <poll.h>
#include <fnmatch.h>
#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif
#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif
#include "rsyslog.h"		/* error codes etc... */
#include "dirty.h"
#include "cfsysline.h"		/* access to config file objects */
#include "module-template.h"	/* generic module interface code - very important, read it! */
#include "srUtils.h"		/* some utility functions */
#include "msg.h"
#include "stream.h"
#include "errmsg.h"
#include "glbl.h"
#include "unicode-helper.h"
#include "prop.h"
#include "stringbuf.h"
#include "ruleset.h"
#include "ratelimit.h"

#include <regex.h> // TODO: fix via own module

MODULE_TYPE_INPUT	/* must be present for input modules, do not remove */
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imfile")

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA	/* must be present, starts static data */
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(strm)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)

static int bLegacyCnfModGlobalsPermitted;/* are legacy module-global config parameters permitted? */

#define NUM_MULTISUB 1024 /* default max number of submits */
#define DFLT_PollInterval 10

#define INIT_FILE_TAB_SIZE 4 /* default file table size - is extended as needed, use 2^x value */
#define INIT_FILE_IN_DIR_TAB_SIZE 1 /* initial size for "associated files tab" in directory table */
#define INIT_WDMAP_TAB_SIZE 1 /* default wdMap table size - is extended as needed, use 2^x value */

#define ADD_METADATA_UNSPECIFIED -1

/* this structure is used in pure polling mode as well one of the support
 * structures for inotify.
 */
typedef struct lstn_s {
	struct lstn_s *next, *prev;
	struct lstn_s *masterLstn;/* if dynamic file (via wildcard), this points to the configured
				 * master entry. For master entries, it is always NULL. Only
				 * dynamic files can be deleted from the "files" list. */
	uchar *pszFileName;
	uchar *pszDirName;
	uchar *pszBaseName;
	uchar *pszTag;
	size_t lenTag;
	uchar *pszStateFile; /* file in which state between runs is to be stored (dynamic if NULL) */
	int readTimeout;
	int iFacility;
	int iSeverity;
	int maxLinesAtOnce;
	uint32_t trimLineOverBytes;
	int nRecords; /**< How many records did we process before persisting the stream? */
	int iPersistStateInterval; /**< how often should state be persisted? (0=on close only) */
	strm_t *pStrm;	/* its stream (NULL if not assigned) */
	sbool bRMStateOnDel;
	sbool hasWildcard;
	uint8_t readMode;	/* which mode to use in ReadMulteLine call? */
	uchar *startRegex;	/* regex that signifies end of message (NULL if unset) */
	regex_t end_preg;	/* compiled version of startRegex */
	uchar *prevLineSegment;	/* previous line segment (in regex mode) */
	sbool escapeLF;	/* escape LF inside the MSG content? */
	sbool reopenOnTruncate;
	sbool addMetadata;
	sbool addCeeTag;
	sbool freshStartTail; /* read from tail of file on fresh start? */
	ruleset_t *pRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	ratelimit_t *ratelimiter;
	multi_submit_t multiSub;
} lstn_t;

static struct configSettings_s {
	uchar *pszFileName;
	uchar *pszFileTag;
	uchar *pszStateFile;
	uchar *pszBindRuleset;
	int iPollInterval;
	int iPersistStateInterval;	/* how often if state file to be persisted? (default 0->never) */
	int iFacility; /* local0 */
	int iSeverity;  /* notice, as of rfc 3164 */
	int readMode;  /* mode to use for ReadMultiLine call */
	int64 maxLinesAtOnce;	/* how many lines to process in a row? */
	uint32_t trimLineOverBytes;  /* 0: never trim line, positive number: trim line if over bytes */
} cs;

struct instanceConf_s {
	uchar *pszFileName;
	uchar *pszDirName;
	uchar *pszFileBaseName;
	uchar *pszTag;
	uchar *pszStateFile;
	uchar *pszBindRuleset;
	int nMultiSub;
	int iPersistStateInterval;
	int iFacility;
	int iSeverity;
	int readTimeout;
	sbool bRMStateOnDel;
	uint8_t readMode;
	uchar *startRegex;
	sbool escapeLF;
	sbool reopenOnTruncate;
	sbool addCeeTag;
	sbool addMetadata;
	sbool freshStartTail;
	int maxLinesAtOnce;
	uint32_t trimLineOverBytes;
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	struct instanceConf_s *next;
};


/* forward definitions */
static rsRetVal persistStrmState(lstn_t *pInfo);
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);


#define OPMODE_POLLING 0
#define OPMODE_INOTIFY 1

/* config variables */
struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
	int iPollInterval;	/* number of seconds to sleep when there was no file activity */
	int readTimeout;
	int timeoutGranularity;		/* value in ms */
	instanceConf_t *root, *tail;
	lstn_t *pRootLstn;
	lstn_t *pTailLstn;
	uint8_t opMode;
	sbool configSetViaV2Method;
	sbool haveReadTimeouts;	/* use special processing if read timeouts exist */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

#ifdef HAVE_INOTIFY_INIT
/* support for inotify mode */

/* we need to track directories */
struct dirInfoFiles_s { /* associated files */
	lstn_t *pLstn;
	int refcnt;	/* due to inotify's async nature, we may have multiple
			 * references to a single file inside our cache - e.g. when
			 * inodes are removed, and the file name is re-created BUT another
			 * process (like rsyslogd ;)) holds open the old inode.
			 */
};
typedef struct dirInfoFiles_s dirInfoFiles_t;

/* This structure is a dynamic table to track file entries */
struct fileTable_s {
	dirInfoFiles_t *listeners;
	int currMax;
	int allocMax;
};
typedef struct fileTable_s fileTable_t;

/* The dirs table (defined below) contains one entry for each directory that
 * is to be monitored. For each directory, it contains array which point to
 * the associated *active* files as well as *configured* files. Note that
 * the configured files may currently not exist, but will be processed
 * when they are created.
 */
struct dirInfo_s {
	uchar *dirName;
	fileTable_t active; /* associated active files */
	fileTable_t configured; /* associated configured files */
};
typedef struct dirInfo_s dirInfo_t;
static dirInfo_t *dirs = NULL;
static int allocMaxDirs;
static int currMaxDirs;
/* the following two macros are used to select the correct file table */
#define ACTIVE_FILE 1
#define CONFIGURED_FILE 0


/* We need to map watch descriptors to our actual objects. Unfortunately, the
 * inotify API does not provide us with any cookie, so a simple O(1) algorithm
 * cannot be done (what a shame...). We assume that maintaining the array is much
 * less often done than looking it up, so we keep the array sorted by watch descriptor
 * and do a binary search on the wd we get back. This is at least O(log n), which
 * is not too bad for the anticipated use case.
 */
struct wd_map_s {
	int wd;		/* ascending sort key */
	lstn_t *pLstn;	/* NULL, if this is a dir entry, otherwise pointer into listener(file) table */
	int dirIdx;	/* index into dirs table, undefined if pLstn == NULL */
	time_t timeoutBase; /* what time to calculate the timeout against? */
};
typedef struct wd_map_s wd_map_t;
static wd_map_t *wdmap = NULL;
static int nWdmap;
static int allocMaxWdmap;
static int ino_fd;	/* fd for inotify calls */

#endif /* #if HAVE_INOTIFY_INIT -------------------------------------------------- */

static prop_t *pInputName = NULL;	/* there is only one global inputName for all messages generated by this input */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "pollinginterval", eCmdHdlrPositiveInt, 0 },
	{ "readtimeout", eCmdHdlrPositiveInt, 0 },
	{ "timeoutgranularity", eCmdHdlrPositiveInt, 0 },
	{ "mode", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {
	{ "file", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "tag", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "severity", eCmdHdlrSeverity, 0 },
	{ "facility", eCmdHdlrFacility, 0 },
	{ "ruleset", eCmdHdlrString, 0 },
	{ "readmode", eCmdHdlrInt, 0 },
	{ "startmsg.regex", eCmdHdlrString, 0 },
	{ "escapelf", eCmdHdlrBinary, 0 },
	{ "reopenontruncate", eCmdHdlrBinary, 0 },
	{ "maxlinesatonce", eCmdHdlrInt, 0 },
	{ "trimlineoverbytes", eCmdHdlrInt, 0 },
	{ "maxsubmitatonce", eCmdHdlrInt, 0 },
	{ "removestateondelete", eCmdHdlrBinary, 0 },
	{ "persiststateinterval", eCmdHdlrInt, 0 },
	{ "deletestateonfiledelete", eCmdHdlrBinary, 0 },
	{ "addmetadata", eCmdHdlrBinary, 0 },
	{ "addceetag", eCmdHdlrBinary, 0 },
	{ "statefile", eCmdHdlrString, CNFPARAM_DEPRECATED },
	{ "readtimeout", eCmdHdlrPositiveInt, 0 },
	{ "freshstarttail", eCmdHdlrBinary, 0}
};
static struct cnfparamblk inppblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(inppdescr)/sizeof(struct cnfparamdescr),
	  inppdescr
	};

#include "im-helper.h" /* must be included AFTER the type definitions! */


#ifdef HAVE_INOTIFY_INIT
/* support for inotify mode */

#if 0 /* enable if you need this for debugging */
static void
dbg_wdmapPrint(char *msg)
{
	int i;
	DBGPRINTF("%s\n", msg);
	for(i = 0 ; i < nWdmap ; ++i)
		DBGPRINTF("wdmap[%d]: wd: %d, file %d, dir %d\n", i,
			  wdmap[i].wd, wdmap[i].fIdx, wdmap[i].dirIdx);
}
#endif

static rsRetVal
wdmapInit(void)
{
	DEFiRet;
	free(wdmap);
	CHKmalloc(wdmap = malloc(sizeof(wd_map_t) * INIT_WDMAP_TAB_SIZE));
	allocMaxWdmap = INIT_WDMAP_TAB_SIZE;
	nWdmap = 0;
finalize_it:
	RETiRet;
}

/* looks up a wdmap entry by pLstn pointer and returns it's index if found
 * or -1 if not found.
 */
static int
wdmapLookupListner(lstn_t* pLstn)
{
	int i = 0;
	int wd = -1;
	/* Loop through */
	for(i = 0 ; i < nWdmap; ++i) {
		if (wdmap[i].pLstn == pLstn)
			wd = wdmap[i].wd;
	}

	return wd;
}

/* looks up a wdmap entry by dirIdx and returns it's index if found
 * or -1 if not found.
 */
static int
wdmapLookupListnerIdx(const int dirIdx)
{
	int i = 0;
	int wd = -1;
	/* Loop through */
	for(i = 0 ; i < nWdmap; ++i) {
		if (wdmap[i].dirIdx == dirIdx)
			wd = wdmap[i].wd;
	}

	return wd;
}

/* compare function for bsearch() */
static int
wdmap_cmp(const void *k, const void *a)
{
	int key = *((int*) k);
	wd_map_t *etry = (wd_map_t*) a;
	if(key < etry->wd)
		return -1;
	else if(key > etry->wd)
		return 1;
	else
		return 0;
}
/* looks up a wdmap entry and returns it's index if found
 * or -1 if not found.
 */
static wd_map_t *
wdmapLookup(int wd)
{
	return bsearch(&wd, wdmap, nWdmap, sizeof(wd_map_t), wdmap_cmp);
}

/* note: we search backwards, as inotify tends to return increasing wd's */
static rsRetVal
wdmapAdd(int wd, const int dirIdx, lstn_t *const pLstn)
{
	wd_map_t *newmap;
	int newmapsize;
	int i;
	DEFiRet;

	for(i = nWdmap-1 ; i >= 0 && wdmap[i].wd > wd ; --i)
		; 	/* just scan */
	if(i >= 0 && wdmap[i].wd == wd) {
		DBGPRINTF("imfile: wd %d already in wdmap!\n", wd);
		ABORT_FINALIZE(RS_RET_FILE_ALREADY_IN_TABLE);
	}
	++i;
	/* i now points to the entry that is to be moved upwards (or end of map) */
	if(nWdmap == allocMaxWdmap) {
		newmapsize = 2 * allocMaxWdmap;
		CHKmalloc(newmap = realloc(wdmap, sizeof(wd_map_t) * newmapsize));
		// TODO: handle the error more intelligently? At all possible? -- 2013-10-15
		wdmap = newmap;
		allocMaxWdmap = newmapsize;
	}
	if(i < nWdmap) {
		/* we need to shift to make room for new entry */
		memmove(wdmap + i + 1, wdmap + i, sizeof(wd_map_t) * (nWdmap - i));
	}
	wdmap[i].wd = wd;
	wdmap[i].dirIdx = dirIdx;
	wdmap[i].pLstn = pLstn;
	++nWdmap;
	DBGPRINTF("imfile: enter into wdmap[%d]: wd %d, dir %d, lstn %s:%s\n",i,wd,dirIdx,
		  (pLstn == NULL) ? "DIRECTORY" : "FILE",
	          (pLstn == NULL) ? dirs[dirIdx].dirName : pLstn->pszFileName);

finalize_it:
	RETiRet;
}

static rsRetVal
wdmapDel(const int wd)
{
	int i;
	DEFiRet;

	for(i = 0 ; i < nWdmap && wdmap[i].wd < wd ; ++i)
		; 	/* just scan */
	if(i == nWdmap ||  wdmap[i].wd != wd) {
		DBGPRINTF("imfile: wd %d shall be deleted but not in wdmap!\n", wd);
		FINALIZE;
	}

	if(i < nWdmap-1) {
		/* we need to shift to delete it (see comment at wdmap definition) */
		memmove(wdmap + i, wdmap + i + 1, sizeof(wd_map_t) * (nWdmap - i - 1));
	}
	--nWdmap;
	DBGPRINTF("imfile: wd %d deleted, was idx %d\n", wd, i);

finalize_it:
	RETiRet;
}

#endif /* #if HAVE_INOTIFY_INIT */


/*
*	Helper function to combine statefile and workdir
*/
static int 
getFullStateFileName(uchar* pszstatefile, uchar* pszout, int ilenout)
{
	int lenout; 
	const uchar* pszworkdir; 

	/* Get Raw Workdir, if it is NULL we need to propper handle it */
	pszworkdir = glblGetWorkDirRaw(); 

	/* Construct file name */
	lenout = snprintf((char*)pszout, ilenout, "%s/%s",
			     (char*) (pszworkdir == NULL ? "." : (char*) pszworkdir), (char*)pszstatefile);

	/* return out length */
	return lenout; 
}


/* this generates a state file name suitable for the current file. To avoid
 * malloc calls, it must be passed a buffer which should be MAXFNAME large.
 * Note: the buffer is not necessarily populated ... always ONLY use the
 * RETURN VALUE!
 */
static uchar *
getStateFileName(lstn_t *const __restrict__ pLstn,
	 	 uchar *const __restrict__ buf,
		 const size_t lenbuf)
{
	uchar *ret;
	if(pLstn->pszStateFile == NULL) {
		snprintf((char*)buf, lenbuf - 1, "imfile-state:%s", pLstn->pszFileName);
		buf[lenbuf-1] = '\0'; /* be on the safe side... */
		uchar *p = buf;
		for( ; *p ; ++p) {
			if(*p == '/')
				*p = '-';
		}
		ret = buf;
	} else {
		ret = pLstn->pszStateFile;
	}
	return ret;
}


/* enqueue the read file line as a message. The provided string is
 * not freed - thuis must be done by the caller.
 */
static rsRetVal enqLine(lstn_t *const __restrict__ pLstn,
			cstr_t *const __restrict__ cstrLine)
{
	DEFiRet;
	smsg_t *pMsg;

	if(rsCStrLen(cstrLine) == 0) {
		/* we do not process empty lines */
		FINALIZE;
	}

	CHKiRet(msgConstruct(&pMsg));
	MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
	MsgSetInputName(pMsg, pInputName);
	if (pLstn->addCeeTag) {
		size_t msgLen = cstrLen(cstrLine);
		const char *const ceeToken = "@cee:";
		size_t ceeMsgSize = msgLen + strlen(ceeToken) +1;
		char *ceeMsg;
		CHKmalloc(ceeMsg = MALLOC(ceeMsgSize));
		strcpy(ceeMsg, ceeToken);
		strcat(ceeMsg, (char*)rsCStrGetSzStrNoNULL(cstrLine));
		MsgSetRawMsg(pMsg, ceeMsg, ceeMsgSize);
		free(ceeMsg);
	} else {
		MsgSetRawMsg(pMsg, (char*)rsCStrGetSzStrNoNULL(cstrLine), cstrLen(cstrLine));
	}
	MsgSetMSGoffs(pMsg, 0);	/* we do not have a header... */
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetTAG(pMsg, pLstn->pszTag, pLstn->lenTag);
	msgSetPRI(pMsg, pLstn->iFacility | pLstn->iSeverity);
	MsgSetRuleset(pMsg, pLstn->pRuleset);
	if(pLstn->addMetadata)
		msgAddMetadata(pMsg, (uchar*)"filename", pLstn->pszFileName);
	ratelimitAddMsg(pLstn->ratelimiter, &pLstn->multiSub, pMsg);
finalize_it:
	RETiRet;
}


/* try to open a file which has a state file. If the state file does not
 * exist or cannot be read, an error is returned.
 */
static rsRetVal
openFileWithStateFile(lstn_t *const __restrict__ pLstn)
{
	DEFiRet;
	strm_t *psSF = NULL;
	uchar pszSFNam[MAXFNAME];
	size_t lenSFNam;
	struct stat stat_buf;
	uchar statefile[MAXFNAME];

	uchar *const statefn = getStateFileName(pLstn, statefile, sizeof(statefile));
	DBGPRINTF("imfile: trying to open state for '%s', state file '%s'\n",
		  pLstn->pszFileName, statefn);

	/* Get full path and file name */
	lenSFNam = getFullStateFileName(statefn, pszSFNam, sizeof(pszSFNam)); 

	/* check if the file exists */
	if(stat((char*) pszSFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			DBGPRINTF("imfile: NO state file (%s) exists for '%s'\n", pszSFNam, pLstn->pszFileName);
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("imfile: error trying to access state file for '%s':%s\n",
			          pLstn->pszFileName, errStr);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}

	/* If we reach this point, we have a state file */

	CHKiRet(strm.Construct(&psSF));
	CHKiRet(strm.SettOperationsMode(psSF, STREAMMODE_READ));
	CHKiRet(strm.SetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetFName(psSF, pszSFNam, lenSFNam));
	CHKiRet(strm.ConstructFinalize(psSF));

	/* read back in the object */
	CHKiRet(obj.Deserialize(&pLstn->pStrm, (uchar*) "strm", psSF, NULL, pLstn));
	DBGPRINTF("imfile: deserialized state file, state file base name '%s', "
		  "configured base name '%s'\n", pLstn->pStrm->pszFName,
		  pLstn->pszFileName);
	if(ustrcmp(pLstn->pStrm->pszFName, pLstn->pszFileName)) {
		errmsg.LogError(0, RS_RET_STATEFILE_WRONG_FNAME, "imfile: state file '%s' "
				"contains file name '%s', but is used for file '%s'. State "
				"file deleted, starting from begin of file.",
				pszSFNam, pLstn->pStrm->pszFName, pLstn->pszFileName);

		unlink((char*)pszSFNam);
		ABORT_FINALIZE(RS_RET_STATEFILE_WRONG_FNAME);
	}

	strm.CheckFileChange(pLstn->pStrm);
	CHKiRet(strm.SeekCurrOffs(pLstn->pStrm));

	/* note: we do not delete the state file, so that the last position remains
	 * known even in the case that rsyslogd aborts for some reason (like powerfail)
	 */

finalize_it:
	if(psSF != NULL)
		strm.Destruct(&psSF);

	RETiRet;
}

/* try to open a file for which no state file exists. This function does NOT
 * check if a state file actually exists or not -- this must have been
 * checked before calling it.
 */
static rsRetVal
openFileWithoutStateFile(lstn_t *const __restrict__ pLstn)
{
	DEFiRet;
	struct stat stat_buf;

	DBGPRINTF("imfile: clean startup withOUT state file for '%s'\n", pLstn->pszFileName);
	if(pLstn->pStrm != NULL)
		strm.Destruct(&pLstn->pStrm);
	CHKiRet(strm.Construct(&pLstn->pStrm));
	CHKiRet(strm.SettOperationsMode(pLstn->pStrm, STREAMMODE_READ));
	CHKiRet(strm.SetsType(pLstn->pStrm, STREAMTYPE_FILE_MONITOR));
	CHKiRet(strm.SetFName(pLstn->pStrm, pLstn->pszFileName, strlen((char*) pLstn->pszFileName)));
	CHKiRet(strm.ConstructFinalize(pLstn->pStrm));

	/* As a state file not exist, this is a fresh start. seek to file end
	 * when freshStartTail is on.
	 */
	if(pLstn->freshStartTail){
		if(stat((char*) pLstn->pszFileName, &stat_buf) != -1) {
			pLstn->pStrm->iCurrOffs = stat_buf.st_size;
			CHKiRet(strm.SeekCurrOffs(pLstn->pStrm));
		}
	}
	strmSetReadTimeout(pLstn->pStrm, pLstn->readTimeout);

finalize_it:
	RETiRet;
}
/* try to open a file. This involves checking if there is a status file and,
 * if so, reading it in. Processing continues from the last know location.
 */
static rsRetVal
openFile(lstn_t *const __restrict__ pLstn)
{
	DEFiRet;

	CHKiRet_Hdlr(openFileWithStateFile(pLstn)) {
		CHKiRet(openFileWithoutStateFile(pLstn));
	}

	DBGPRINTF("imfile: breopenOnTruncate %d for '%s'\n",
		pLstn->reopenOnTruncate, pLstn->pszFileName);
	CHKiRet(strm.SetbReopenOnTruncate(pLstn->pStrm, pLstn->reopenOnTruncate));

finalize_it:
	RETiRet;
}


/* The following is a cancel cleanup handler for strmReadLine(). It is necessary in case
 * strmReadLine() is cancelled while processing the stream. -- rgerhards, 2008-03-27
 */
static void pollFileCancelCleanup(void *pArg)
{
	BEGINfunc;
	cstr_t **ppCStr = (cstr_t**) pArg;
	if(*ppCStr != NULL)
		rsCStrDestruct(ppCStr);
	ENDfunc;
}


/* poll a file, need to check file rollover etc. open file if not open */
#if !defined(_AIX)
#pragma GCC diagnostic ignored "-Wempty-body"
#endif
static rsRetVal
pollFile(lstn_t *pLstn, int *pbHadFileData)
{
	cstr_t *pCStr = NULL;
	DEFiRet;

	/* Note: we must do pthread_cleanup_push() immediately, because the POXIS macros
	 * otherwise do not work if I include the _cleanup_pop() inside an if... -- rgerhards, 2008-08-14
	 */
	pthread_cleanup_push(pollFileCancelCleanup, &pCStr);
	int nProcessed = 0;
	if(pLstn->pStrm == NULL) {
		CHKiRet(openFile(pLstn)); /* open file */
	}

	/* loop below will be exited when strmReadLine() returns EOF */
	while(glbl.GetGlobalInputTermState() == 0) {
		if(pLstn->maxLinesAtOnce != 0 && nProcessed >= pLstn->maxLinesAtOnce)
			break;
		if(pLstn->startRegex == NULL) {
			CHKiRet(strm.ReadLine(pLstn->pStrm, &pCStr, pLstn->readMode, pLstn->escapeLF, pLstn->trimLineOverBytes));
		} else {
			CHKiRet(strmReadMultiLine(pLstn->pStrm, &pCStr, &pLstn->end_preg, pLstn->escapeLF));
		}
		++nProcessed;
		if(pbHadFileData != NULL)
			*pbHadFileData = 1; /* this is just a flag, so set it and forget it */
		CHKiRet(enqLine(pLstn, pCStr)); /* process line */
		rsCStrDestruct(&pCStr); /* discard string (must be done by us!) */
		if(pLstn->iPersistStateInterval > 0 && pLstn->nRecords++ >= pLstn->iPersistStateInterval) {
			persistStrmState(pLstn);
			pLstn->nRecords = 0;
		}
	}

finalize_it:
	multiSubmitFlush(&pLstn->multiSub);
	pthread_cleanup_pop(0);

	if(pCStr != NULL) {
		rsCStrDestruct(&pCStr);
	}

	RETiRet;
}
#if !defined(_AIX)
#pragma GCC diagnostic warning "-Wempty-body"
#endif


/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal
createInstance(instanceConf_t **pinst)
{
	instanceConf_t *inst;
	DEFiRet;
	CHKmalloc(inst = MALLOC(sizeof(instanceConf_t)));
	inst->next = NULL;
	inst->pBindRuleset = NULL;

	inst->pszBindRuleset = NULL;
	inst->pszFileName = NULL;
	inst->pszTag = NULL;
	inst->pszStateFile = NULL;
	inst->nMultiSub = NUM_MULTISUB;
	inst->iSeverity = 5;
	inst->iFacility = 128;
	inst->maxLinesAtOnce = 0;
	inst->trimLineOverBytes = 0;
	inst->iPersistStateInterval = 0;
	inst->readMode = 0;
	inst->startRegex = NULL;
	inst->bRMStateOnDel = 1;
	inst->escapeLF = 1;
	inst->reopenOnTruncate = 0;
	inst->addMetadata = ADD_METADATA_UNSPECIFIED;
	inst->addCeeTag = 0;
	inst->freshStartTail = 0;
	inst->readTimeout = loadModConf->readTimeout;

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = inst;
	} else {
		loadModConf->tail->next = inst;
		loadModConf->tail = inst;
	}

	*pinst = inst;
finalize_it:
	RETiRet;
}


/* the basen(ame) buffer must be of size MAXFNAME
 * returns the index of the slash in front of basename
 */
static int
getBasename(uchar *const __restrict__ basen, uchar *const __restrict__ path)
{
	int i;
	int found = 0;
	const int lenName = ustrlen(path);
	for(i = lenName ; i >= 0 ; --i) {
		if(path[i] == '/') {
			/* found basename component */
			found = 1;
			if(i == lenName)
				basen[0] = '\0';
			else {
				memcpy(basen, path+i+1, lenName-i);
			}
			break;
		}
	}
	if (found == 1)
		return i;
	else {
		return -1;
	}
}

/* the basedir buffer must be of size MAXFNAME
 * returns the index of the last slash before the basename
 */
static int
getBasedir(uchar *const __restrict__ basedir, uchar *const __restrict__ path)
{
	int i;
	int found = 0;
	const int lenName = ustrlen(path);
	for(i = lenName ; i >= 0 ; --i) {
		if(path[i] == '/') {
			/* found basename component */
			found = 1;
			if(i == lenName)
				basedir[0] = '\0';
			else {
				memcpy(basedir, path, i);
				basedir[i] = '\0';
			}
			break;
		}
	}
	if (found == 1)
		return i;
	else {
		return -1;
	}
}

/* this function checks instance parameters and does some required pre-processing
 * (e.g. split filename in path and actual name)
 * Note: we do NOT use dirname()/basename() as they have portability problems.
 */
static rsRetVal
checkInstance(instanceConf_t *inst)
{
	char dirn[MAXFNAME];
	uchar basen[MAXFNAME];
	int i;
	struct stat sb;
	int r;
	int eno;
	char errStr[512];
	DEFiRet;

	/* this is primarily for the clang static analyzer, but also
	 * guards against logic errors in the config handler.
	 */
	if(inst->pszFileName == NULL)
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);

	i = getBasename(basen, inst->pszFileName);
	if (i == -1) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile: file path '%s' does not include a basename component",
			inst->pszFileName);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	
	memcpy(dirn, inst->pszFileName, i); /* do not copy slash */
	dirn[i] = '\0';
	CHKmalloc(inst->pszFileBaseName = (uchar*) strdup((char*)basen));
	CHKmalloc(inst->pszDirName = (uchar*) strdup(dirn));

	if(dirn[0] == '\0') {
		dirn[0] = '/';
		dirn[1] = '\0';
	}
	r = stat(dirn, &sb);
	if(r != 0)  {
		eno = errno;
		rs_strerror_r(eno, errStr, sizeof(errStr));
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile warning: directory '%s': %s",
				dirn, errStr);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(!S_ISDIR(sb.st_mode)) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile warning: configured directory "
				"'%s' is NOT a directory", dirn);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

finalize_it:
	RETiRet;
}


/* add a new monitor */
static rsRetVal
addInstance(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	instanceConf_t *inst;
	DEFiRet;

	if(cs.pszFileName == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no file name given, file monitor can not be created");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(cs.pszFileTag == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no tag value given, file monitor can not be created");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	CHKiRet(createInstance(&inst));
	if((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
		inst->pszBindRuleset = NULL;
	} else {
		CHKmalloc(inst->pszBindRuleset = ustrdup(cs.pszBindRuleset));
	}
	inst->pszFileName = (uchar*) strdup((char*) cs.pszFileName);
	inst->pszTag = (uchar*) strdup((char*) cs.pszFileTag);
	inst->pszStateFile = cs.pszStateFile == NULL ? NULL : (uchar*) strdup((char*) cs.pszStateFile);
	inst->iSeverity = cs.iSeverity;
	inst->iFacility = cs.iFacility;
	if(cs.maxLinesAtOnce) {
		if(loadModConf->opMode == OPMODE_INOTIFY) {
			errmsg.LogError(0, RS_RET_PARAM_NOT_PERMITTED,
				"parameter \"maxLinesAtOnce\" not "
				"permited in inotify mode - ignored");
		} else {
			inst->maxLinesAtOnce = cs.maxLinesAtOnce;
		}
	}
	inst->trimLineOverBytes = cs.trimLineOverBytes;
	inst->iPersistStateInterval = cs.iPersistStateInterval;
	inst->readMode = cs.readMode;
	inst->escapeLF = 0;
	inst->reopenOnTruncate = 0;
	inst->addMetadata = 0;
	inst->addCeeTag = 0;
	inst->bRMStateOnDel = 0;
	inst->readTimeout = loadModConf->readTimeout;

	CHKiRet(checkInstance(inst));

	/* reset legacy system */
	cs.iPersistStateInterval = 0;
	resetConfigVariables(NULL, NULL); /* values are both dummies */

finalize_it:
	free(pNewVal); /* we do not need it, but we must free it! */
	RETiRet;
}


/* This adds a new listener object to the bottom of the list, but
 * it does NOT initialize any data members except for the list
 * pointers themselves.
 */
static rsRetVal
lstnAdd(lstn_t **newLstn)
{
	lstn_t *pLstn;
	DEFiRet;

	CHKmalloc(pLstn = (lstn_t*) MALLOC(sizeof(lstn_t)));
	if(runModConf->pRootLstn == NULL) {
		runModConf->pRootLstn = pLstn;
		pLstn->prev = NULL;
	} else {
		runModConf->pTailLstn->next = pLstn;
		pLstn->prev = runModConf->pTailLstn;
	}
	runModConf->pTailLstn = pLstn;
	pLstn->next = NULL;
	*newLstn = pLstn;

finalize_it:
	RETiRet;
}

/* delete a listener object */
static void
lstnDel(lstn_t *pLstn)
{
	DBGPRINTF("imfile: lstnDel called for %s\n", pLstn->pszFileName);
	if(pLstn->pStrm != NULL) { /* stream open? */
		persistStrmState(pLstn);
		strm.Destruct(&(pLstn->pStrm));
	}
	ratelimitDestruct(pLstn->ratelimiter);
	free(pLstn->multiSub.ppMsgs);
	free(pLstn->pszFileName);
	free(pLstn->pszTag);
	free(pLstn->pszStateFile);
	free(pLstn->pszBaseName);
	if(pLstn->startRegex != NULL)
		regfree(&pLstn->end_preg);

	if(pLstn == runModConf->pRootLstn)
		runModConf->pRootLstn = pLstn->next;
	if(pLstn == runModConf->pTailLstn)
		runModConf->pTailLstn = pLstn->prev;
	if(pLstn->next != NULL)
		pLstn->next->prev = pLstn->prev;
	if(pLstn->prev != NULL)
		pLstn->prev->next = pLstn->next;
	free(pLstn);
}

/* This function is called when a new listener shall be added.
 * It also does some late stage error checking on the config
 * and reports issues it finds.
 */
static rsRetVal
addListner(instanceConf_t *inst)
{
	DEFiRet;
	lstn_t *pThis;
	sbool hasWildcard;

	hasWildcard = containsGlobWildcard((char*)inst->pszFileBaseName);
	if(hasWildcard) {
		if(runModConf->opMode == OPMODE_POLLING) {
			errmsg.LogError(0, RS_RET_IMFILE_WILDCARD,
				"imfile: The to-be-monitored file \"%s\" contains "
				"wildcards. This is not supported in "
				"polling mode.", inst->pszFileName);
			ABORT_FINALIZE(RS_RET_IMFILE_WILDCARD);
		} else if(inst->pszStateFile != NULL) {
			errmsg.LogError(0, RS_RET_IMFILE_WILDCARD,
				"imfile: warning: it looks like to-be-monitored "
				"file \"%s\" contains wildcards. This usually "
				"does not work well with specifying a state file.",
				inst->pszFileName);
		}
	}

	CHKiRet(lstnAdd(&pThis));
	pThis->hasWildcard = hasWildcard;
	pThis->pszFileName = (uchar*) strdup((char*) inst->pszFileName);
	pThis->pszDirName = inst->pszDirName; /* use memory from inst! */
	pThis->pszBaseName = (uchar*)strdup((char*)inst->pszFileBaseName); /* be consistent with expanded wildcards! */
	pThis->pszTag = (uchar*) strdup((char*) inst->pszTag);
	pThis->lenTag = ustrlen(pThis->pszTag);
	pThis->pszStateFile = inst->pszStateFile == NULL ? NULL : (uchar*) strdup((char*) inst->pszStateFile);

	CHKiRet(ratelimitNew(&pThis->ratelimiter, "imfile", (char*)inst->pszFileName));
	CHKmalloc(pThis->multiSub.ppMsgs = MALLOC(inst->nMultiSub * sizeof(smsg_t *)));
	pThis->multiSub.maxElem = inst->nMultiSub;
	pThis->multiSub.nElem = 0;
	pThis->iSeverity = inst->iSeverity;
	pThis->iFacility = inst->iFacility;
	pThis->maxLinesAtOnce = inst->maxLinesAtOnce;
	pThis->trimLineOverBytes = inst->trimLineOverBytes;
	pThis->iPersistStateInterval = inst->iPersistStateInterval;
	pThis->readMode = inst->readMode;
	pThis->startRegex = inst->startRegex; /* no strdup, as it is read-only */
	if(pThis->startRegex != NULL)
		if(regcomp(&pThis->end_preg, (char*)pThis->startRegex, REG_EXTENDED)) {
			DBGPRINTF("imfile: error regex compile\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}
	pThis->bRMStateOnDel = inst->bRMStateOnDel;
	pThis->escapeLF = inst->escapeLF;
	pThis->reopenOnTruncate = inst->reopenOnTruncate;
	pThis->addMetadata = (inst->addMetadata == ADD_METADATA_UNSPECIFIED) ?
			       hasWildcard : inst->addMetadata;
	pThis->addCeeTag = inst->addCeeTag;
	pThis->readTimeout = inst->readTimeout;
	pThis->freshStartTail = inst->freshStartTail;
	pThis->pRuleset = inst->pBindRuleset;
	pThis->nRecords = 0;
	pThis->pStrm = NULL;
	pThis->prevLineSegment = NULL;
	pThis->masterLstn = NULL; /* we *are* a master! */
finalize_it:
	RETiRet;
}


BEGINnewInpInst
	struct cnfparamvals *pvals;
	instanceConf_t *inst;
	int i;
CODESTARTnewInpInst
	DBGPRINTF("newInpInst (imfile)\n");

	pvals = nvlstGetParams(lst, &inppblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		DBGPRINTF("input param blk in imfile:\n");
		cnfparamsPrint(&inppblk, pvals);
	}

	CHKiRet(createInstance(&inst));

	for(i = 0 ; i < inppblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(inppblk.descr[i].name, "file")) {
			inst->pszFileName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "statefile")) {
			inst->pszStateFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "removestateondelete")) {
			inst->bRMStateOnDel = (uint8_t) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "tag")) {
			inst->pszTag = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "severity")) {
			inst->iSeverity = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "facility")) {
			inst->iFacility = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "readmode")) {
			inst->readMode = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "startmsg.regex")) {
			inst->startRegex = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "deletestateonfiledelete")) {
			inst->bRMStateOnDel = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "addmetadata")) {
			inst->addMetadata = (sbool) pvals[i].val.d.n;
		} else if (!strcmp(inppblk.descr[i].name, "addceetag")) {
			inst->addCeeTag = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "freshstarttail")) {
			inst->freshStartTail = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "escapelf")) {
			inst->escapeLF = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "reopenontruncate")) {
			inst->reopenOnTruncate = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "maxlinesatonce")) {
			if(   loadModConf->opMode == OPMODE_INOTIFY
			   && pvals[i].val.d.n > 0) {
				errmsg.LogError(0, RS_RET_PARAM_NOT_PERMITTED,
					"parameter \"maxLinesAtOnce\" not "
					"permited in inotify mode - ignored");
			} else {
				inst->maxLinesAtOnce = pvals[i].val.d.n;
			}
		} else if(!strcmp(inppblk.descr[i].name, "trimlineoverbytes")) {
			inst->trimLineOverBytes = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "persiststateinterval")) {
			inst->iPersistStateInterval = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "maxsubmitatonce")) {
			inst->nMultiSub = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "readtimeout")) {
			inst->readTimeout = pvals[i].val.d.n;
		} else {
			DBGPRINTF("imfile: program error, non-handled "
			  "param '%s'\n", inppblk.descr[i].name);
		}
	}
	if(inst->readMode != 0 &&  inst->startRegex != NULL) {
		errmsg.LogError(0, RS_RET_PARAM_NOT_PERMITTED,
			"readMode and startmsg.regex cannot be set "
			"at the same time --- remove one of them");
			ABORT_FINALIZE(RS_RET_PARAM_NOT_PERMITTED);
	}
	if(inst->readTimeout != 0)
		loadModConf->haveReadTimeouts = 1;
	CHKiRet(checkInstance(inst));
finalize_it:
CODE_STD_FINALIZERnewInpInst
	cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	/* init our settings */
	loadModConf->opMode = OPMODE_POLLING;
	loadModConf->iPollInterval = DFLT_PollInterval;
	loadModConf->configSetViaV2Method = 0;
	loadModConf->readTimeout = 0; /* default: no timeout */
	loadModConf->timeoutGranularity = 1000; /* default: 1 second */
	loadModConf->haveReadTimeouts = 0; /* default: no timeout */
	bLegacyCnfModGlobalsPermitted = 1;
	/* init legacy config vars */
	cs.pszFileName = NULL;
	cs.pszFileTag = NULL;
	cs.pszStateFile = NULL;
	cs.iPollInterval = DFLT_PollInterval;
	cs.iPersistStateInterval = 0;
	cs.iFacility = 128;
	cs.iSeverity = 5;
	cs.readMode = 0;
	cs.maxLinesAtOnce = 10240;
	cs.trimLineOverBytes = 0;
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	loadModConf->opMode = OPMODE_INOTIFY; /* new style config has different default! */
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "imfile: error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		DBGPRINTF("module (global) param blk for imfile:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "pollinginterval")) {
			loadModConf->iPollInterval = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "readtimeout")) {
			loadModConf->readTimeout = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "timeoutgranularity")) {
			/* note: we need ms, thus "* 1000" */
			loadModConf->timeoutGranularity = (int) pvals[i].val.d.n * 1000;
		} else if(!strcmp(modpblk.descr[i].name, "mode")) {
			if(!es_strconstcmp(pvals[i].val.d.estr, "polling"))
				loadModConf->opMode = OPMODE_POLLING;
			else if(!es_strconstcmp(pvals[i].val.d.estr, "inotify"))
				loadModConf->opMode = OPMODE_INOTIFY;
			else {
				char *cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
				errmsg.LogError(0, RS_RET_PARAM_ERROR, "imfile: unknown "
					"mode '%s'", cstr);
				free(cstr);
			}
		} else {
			DBGPRINTF("imfile: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}

	/* remove all of our legacy handlers, as they can not used in addition
	 * the the new-style config method.
	 */
	bLegacyCnfModGlobalsPermitted = 0;
	loadModConf->configSetViaV2Method = 1;

finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
CODESTARTendCnfLoad
	if(!loadModConf->configSetViaV2Method) {
		/* persist module-specific settings from legacy config system */
		loadModConf->iPollInterval = cs.iPollInterval;
	}
	DBGPRINTF("imfile: opmode is %d, polling interval is %d\n",
		  loadModConf->opMode,
		  loadModConf->iPollInterval);

	loadModConf = NULL; /* done loading */
	/* free legacy config vars */
	free(cs.pszFileName);
	free(cs.pszFileTag);
	free(cs.pszStateFile);
ENDendCnfLoad


BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf
	for(inst = pModConf->root ; inst != NULL ; inst = inst->next) {
		std_checkRuleset(pModConf, inst);
	}
	if(pModConf->root == NULL) {
		errmsg.LogError(0, RS_RET_NO_LISTNERS,
				"imfile: no files configured to be monitored - "
				"no input will be gathered");
		iRet = RS_RET_NO_LISTNERS;
	}
ENDcheckCnf


/* note: we do access files AFTER we have dropped privileges. This is
 * intentional, user must make sure the files have the right permissions.
 */
BEGINactivateCnf
	instanceConf_t *inst;
CODESTARTactivateCnf
	runModConf = pModConf;
	runModConf->pRootLstn = NULL,
	runModConf->pTailLstn = NULL;

	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		addListner(inst);
	}

	/* if we could not set up any listeners, there is no point in running... */
	if(runModConf->pRootLstn == 0) {
		errmsg.LogError(0, NO_ERRCODE, "imfile: no file monitors could be started, "
				"input not activated.\n");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}
finalize_it:
ENDactivateCnf


BEGINfreeCnf
	instanceConf_t *inst, *del;
CODESTARTfreeCnf
	for(inst = pModConf->root ; inst != NULL ; ) {
		free(inst->pszBindRuleset);
		free(inst->pszFileName);
		free(inst->pszDirName);
		free(inst->pszFileBaseName);
		free(inst->pszTag);
		free(inst->pszStateFile);
		free(inst->startRegex);
		del = inst;
		inst = inst->next;
		free(del);
	}
ENDfreeCnf


/* Monitor files in traditional polling mode.
 *
 * We go through all files and remember if at least one had data. If so, we do
 * another run (until no data was present in any file). Then we sleep for
 * PollInterval seconds and restart the whole process. This ensures that as
 * long as there is some data present, it will be processed at the fastest
 * possible pace - probably important for busy systmes. If we monitor just a
 * single file, the algorithm is slightly modified. In that case, the sleep
 * hapens immediately. The idea here is that if we have just one file, we
 * returned from the file processer because that file had no additional data.
 * So even if we found some lines, it is highly unlikely to find a new one
 * just now. Trying it would result in a performance-costly additional try
 * which in the very, very vast majority of cases will never find any new
 * lines.
 * On spamming the main queue: keep in mind that it will automatically rate-limit
 * ourselfes if we begin to overrun it. So we really do not need to care here.
 */
static rsRetVal
doPolling(void)
{
	int bHadFileData; /* were there at least one file with data during this run? */
	DEFiRet;
	while(glbl.GetGlobalInputTermState() == 0) {
		do {
			lstn_t *pLstn;
			bHadFileData = 0;
			for(pLstn = runModConf->pRootLstn ; pLstn != NULL ; pLstn = pLstn->next) {
				if(glbl.GetGlobalInputTermState() == 1)
					break; /* terminate input! */
				pollFile(pLstn, &bHadFileData);
			}
		} while(bHadFileData == 1 && glbl.GetGlobalInputTermState() == 0);
		  /* warning: do...while()! */

		/* Note: the additional 10ns wait is vitally important. It guards rsyslog
		 * against totally hogging the CPU if the users selects a polling interval
		 * of 0 seconds. It doesn't hurt any other valid scenario. So do not remove.
		 * rgerhards, 2008-02-14
		 */
		if(glbl.GetGlobalInputTermState() == 0)
			srSleep(runModConf->iPollInterval, 10);
	}

	RETiRet;
}


#ifdef HAVE_INOTIFY_INIT
static rsRetVal
fileTableInit(fileTable_t *const __restrict__ tab, const int nelem)
{
	DEFiRet;
	CHKmalloc(tab->listeners = malloc(sizeof(dirInfoFiles_t) * nelem));
	tab->allocMax = nelem;
	tab->currMax = 0;
finalize_it:
	RETiRet;
}
/* uncomment if needed
static void
fileTableDisplay(fileTable_t *tab)
{
	int f;
	uchar *baseName;
	DBGPRINTF("imfile: dirs.currMaxfiles %d\n", tab->currMax);
	for(f = 0 ; f < tab->currMax ; ++f) {
		baseName = tab->listeners[f].pLstn->pszBaseName;
		DBGPRINTF("imfile: TABLE %p CONTENTS, %d->%p:'%s'\n", tab, f, tab->listeners[f].pLstn, (char*)baseName);
	}
}
*/

static int
fileTableSearch(fileTable_t *const __restrict__ tab, uchar *const __restrict__ fn)
{
	int f;
	uchar *baseName = NULL;
	/* UNCOMMENT FOR DEBUG fileTableDisplay(tab); */
	for(f = 0 ; f < tab->currMax ; ++f) {
		baseName = tab->listeners[f].pLstn->pszBaseName;
		if(!fnmatch((char*)baseName, (char*)fn, FNM_PATHNAME | FNM_PERIOD))
			break; /* found */
	}
	if(f == tab->currMax)
		f = -1;
	DBGPRINTF("imfile: fileTableSearch file '%s' - '%s', found:%d\n", fn, (f == -1) ? NULL : baseName, f);
	return f;
}

static int
fileTableSearchNoWildcard(fileTable_t *const __restrict__ tab, uchar *const __restrict__ fn)
{
	int f;
	uchar *baseName = NULL;
	/* UNCOMMENT FOR DEBUG fileTableDisplay(tab); */
	for(f = 0 ; f < tab->currMax ; ++f) {
		baseName = tab->listeners[f].pLstn->pszBaseName;
		if (strcmp((const char*)baseName, (const char*)fn) == 0)
			break; /* found */
	}
	if(f == tab->currMax)
		f = -1;
	DBGPRINTF("imfile: fileTableSearchNoWildcard file '%s' - '%s', found:%d\n", fn, baseName, f);
	return f;
}

/* add file to file table */
static rsRetVal
fileTableAddFile(fileTable_t *const __restrict__ tab, lstn_t *const __restrict__ pLstn)
{
	int j;
	DEFiRet;
	/* UNCOMMENT FOR DEBUG fileTableDisplay(tab); */
	for(j = 0 ; j < tab->currMax && tab->listeners[j].pLstn != pLstn ; ++j)
		; /* just scan */
	if(j < tab->currMax) {
		++tab->listeners[j].refcnt;
		DBGPRINTF("imfile: file '%s' already registered, refcnt now %d\n",
			pLstn->pszFileName, tab->listeners[j].refcnt);
		FINALIZE;
	}

	if(tab->currMax == tab->allocMax) {
		const int newMax = 2 * tab->allocMax;
		dirInfoFiles_t *newListenerTab = realloc(tab->listeners, newMax * sizeof(dirInfoFiles_t));
		if(newListenerTab == NULL) {
			errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
					"cannot alloc memory to map directory/file relationship "
					"for '%s' - ignoring", pLstn->pszFileName);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		tab->listeners = newListenerTab;
		tab->allocMax = newMax;
		DBGPRINTF("imfile: increased dir table to %d entries\n", allocMaxDirs);
	}

	tab->listeners[tab->currMax].pLstn = pLstn;
	tab->listeners[tab->currMax].refcnt = 1;
	tab->currMax++;
finalize_it:
	RETiRet;
}

/* delete a file from file table */
static rsRetVal
fileTableDelFile(fileTable_t *const __restrict__ tab, lstn_t *const __restrict__ pLstn)
{
	int j;
	DEFiRet;

	for(j = 0 ; j < tab->currMax && tab->listeners[j].pLstn != pLstn ; ++j)
		; /* just scan */
	if(j == tab->currMax) {
		DBGPRINTF("imfile: no association for file '%s'\n", pLstn->pszFileName);
		FINALIZE;
	}
	tab->listeners[j].refcnt--;
	if(tab->listeners[j].refcnt == 0) {
		/* we remove that entry (but we never shrink the table) */
		if(j < tab->currMax - 1) {
			/* entry in middle - need to move others */
			memmove(tab->listeners+j, tab->listeners+j+1,
				(tab->currMax -j-1) * sizeof(dirInfoFiles_t));
		}
		--tab->currMax;
	}
finalize_it:
	RETiRet;
}
/* add entry to dirs array */
static rsRetVal
dirsAdd(uchar *dirName, int* piIndex)
{
	sbool sbAdded; 
	int newMax;
	int i, newindex;
	dirInfo_t *newDirTab;
	DEFiRet;

	/* Set new index to last dir by default, then search for a free spot in dirs array */
	newindex = currMaxDirs; 
	sbAdded = TRUE;
	for(i= 0 ; i < currMaxDirs ; ++i) {
		if (dirs[i].dirName == NULL) {
			newindex = i;
			sbAdded = FALSE; 
			DBGPRINTF("imfile: dirsAdd found free spot at %d, reusing\n", newindex);
			break;
		}
	}
	
	/* Save Index for higher functions */
	if (piIndex != NULL )
		*piIndex = newindex; 

	/* Check if dirstab size needs to be increased */
	if(sbAdded == TRUE && newindex == allocMaxDirs) {
		newMax = 2 * allocMaxDirs;
		newDirTab = realloc(dirs, newMax * sizeof(dirInfo_t));
		if(newDirTab == NULL) {
			errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
					"cannot alloc memory to monitor directory '%s' - ignoring",
					dirName);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		dirs = newDirTab;
		allocMaxDirs = newMax;
		DBGPRINTF("imfile: increased dir table to %d entries\n", allocMaxDirs);
	}

	/* if we reach this point, there is space in the file table for the new entry */
	dirs[newindex].dirName = (uchar*)strdup((char*)dirName); /* Get a copy of the string !*/
	CHKiRet(fileTableInit(&dirs[newindex].active, INIT_FILE_IN_DIR_TAB_SIZE));
	CHKiRet(fileTableInit(&dirs[newindex].configured, INIT_FILE_IN_DIR_TAB_SIZE));

	DBGPRINTF("imfile: dirsAdd added dir %d to dirs table: '%s'\n", newindex, dirName);
	/* Increment only if entry was added and not reused */
	if (sbAdded == TRUE)
		++currMaxDirs;
finalize_it:
	RETiRet;
}


/* checks if a dir name is already inside the dirs array. If so, returns
 * its index. If not present, -1 is returned.
 */
static int
dirsFindDir(uchar *dir)
{
	int i, iFind;
	iFind = -1; 

	/* Try to find directory using for() */
	for(i = 0 ; i < currMaxDirs ; ++i) {
		if (dirs[i].dirName != NULL && ustrcmp(dir, dirs[i].dirName) == 0) {
			iFind = i; 
			break; 
		}
	}
	return iFind;
}

static rsRetVal
dirsInit(void)
{
	instanceConf_t *inst;
	DEFiRet;
DBGPRINTF("imfile: dirsInit\n"); 

	free(dirs);
	CHKmalloc(dirs = malloc(sizeof(dirInfo_t) * INIT_FILE_TAB_SIZE));
	allocMaxDirs = INIT_FILE_TAB_SIZE;
	currMaxDirs = 0;

	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		if(dirsFindDir(inst->pszDirName) == -1)
			dirsAdd(inst->pszDirName, NULL);
	}

finalize_it:
	RETiRet;
}

/* add file to directory (create association)
 * fIdx is index into file table, all other information is pulled from that table.
 * bActive is 1 if the file is to be added to active set, else zero
 */
static rsRetVal
dirsAddFile(lstn_t *__restrict__ pLstn, const int bActive)
{
	int dirIdx;
	dirInfo_t *dir;
	DEFiRet;

	dirIdx = dirsFindDir(pLstn->pszDirName);
	if(dirIdx == -1) {
		errmsg.LogError(0, RS_RET_INTERNAL_ERROR, "imfile: could not find "
			"directory '%s' in dirs array - ignoring",
			pLstn->pszDirName);
		FINALIZE;
	}

	dir = dirs + dirIdx;
	CHKiRet(fileTableAddFile((bActive ? &dir->active : &dir->configured), pLstn));
	DBGPRINTF("imfile: associated file [%s] to directory %d[%s], Active = %d\n",
		pLstn->pszFileName, dirIdx, dir->dirName, bActive);
/* UNCOMMENT FOR DEBUG fileTableDisplay(bActive ? &dir->active : &dir->configured); */
finalize_it:
	RETiRet;
}

static void
in_setupDirWatch(const int dirIdx)
{
	int wd;
	sbool hasWildcard;
	char dirnametrunc[MAXFNAME];
	int dirnamelen = 0; 
	char* psztmp; 

	wd = inotify_add_watch(ino_fd, (char*)dirs[dirIdx].dirName, IN_CREATE|IN_DELETE|IN_MOVED_FROM);
	if(wd < 0) {
		/* check for wildcard in directoryname, if last character is a wildcard we remove it and try again! */
		dirnamelen = ustrlen(dirs[dirIdx].dirName); 
		memcpy(dirnametrunc, dirs[dirIdx].dirName, dirnamelen); /* Copy mem */

		hasWildcard = containsGlobWildcard(dirnametrunc);
		if(hasWildcard) {
			/* Set NULL Byte on last directory delimiter occurrence,
			will also remove asterix */
			psztmp = strrchr(dirnametrunc, '/');
			*psztmp = '\0';
			
			/* Try to add inotify watch again */
			wd = inotify_add_watch(ino_fd, dirnametrunc, IN_CREATE|IN_DELETE|IN_MOVED_FROM);
			if(wd < 0) {
				DBGPRINTF("imfile: in_setupDirWatch: could not create dir watch for '%s' (wildcard) with error %d\n",
					dirs[dirIdx].dirName, errno);
				goto done;
			} else {
				DBGPRINTF("imfile: in_setupDirWatch: Found wildcard at the end of '%s', " 
					"removing and watching parent path instead! \n",
					dirs[dirIdx].dirName);
			}
		} else {
			DBGPRINTF("imfile: in_setupDirWatch: could not create dir watch for '%s' with error %d\n",
				dirs[dirIdx].dirName, errno);
			goto done;
		}
	}
	wdmapAdd(wd, dirIdx, NULL);
	DBGPRINTF("imfile: in_setupDirWatch: watch %d added for dir %s\n", wd, 
		(dirnamelen == 0) ? (char*) dirs[dirIdx].dirName : (char*) dirnametrunc);
done:	return;
}

/* Setup a new file watch for a known active file. It must already have
 * been entered into the correct tables.
 * Note: we need to try to read this file, as it may already contain data this
 * needs to be processed, and we won't get an event for that as notifications
 * happen only for things after the watch has been activated.
 * Note: newFileName is NULL for configured files, and non-NULL for dynamically
 * detected files (e.g. wildcards!)
 */
static void
startLstnFile(lstn_t *const __restrict__ pLstn)
{
	rsRetVal localRet;
	const int wd = inotify_add_watch(ino_fd, (char*)pLstn->pszFileName, IN_MODIFY);
	if(wd < 0) {
		char errStr[512];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		DBGPRINTF("imfile: could not create file table entry for '%s' - "
			  "not processing it now: %s\n",
			  pLstn->pszFileName, errStr);
		goto done;
	}
	if((localRet = wdmapAdd(wd, -1, pLstn)) != RS_RET_OK) {
		DBGPRINTF("imfile: error %d adding file to wdmap, ignoring\n", localRet);
		goto done;
	}
	DBGPRINTF("imfile: watch %d added for file %s\n", wd, pLstn->pszFileName);
	dirsAddFile(pLstn, ACTIVE_FILE);
	pollFile(pLstn, NULL);
done:	return;
}

/* Duplicate an existing listener. This is called when a new file is to
 * be monitored due to wildcard detection. Returns the new pLstn in
 * the ppExisting parameter.
 */
static rsRetVal
lstnDup(lstn_t **ppExisting, uchar *const __restrict__ newname, uchar *const __restrict__ newdirname)
{
	DEFiRet;
	lstn_t *const existing = *ppExisting;
	lstn_t *pThis;
	CHKiRet(lstnAdd(&pThis));
	
	/* Use dynamic dirname if newdirname is set! */
	if (newdirname == NULL) {
		pThis->pszDirName = existing->pszDirName; /* read-only */
	} else {
		pThis->pszDirName = (uchar*)strdup((char*)newdirname); 
	}
	pThis->pszBaseName = (uchar*)strdup((char*)newname);
	if(asprintf((char**)&pThis->pszFileName, "%s/%s", (char*)pThis->pszDirName, (char*)newname) == -1) {
		DBGPRINTF("imfile/lstnDup: asprintf failed, malfunction can happen\n");
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	pThis->pszTag = (uchar*) strdup((char*) existing->pszTag);
	pThis->lenTag = ustrlen(pThis->pszTag);
	pThis->pszStateFile = existing->pszStateFile == NULL ? NULL : (uchar*) strdup((char*) existing->pszStateFile);

	CHKiRet(ratelimitNew(&pThis->ratelimiter, "imfile", (char*)pThis->pszFileName));
	pThis->multiSub.maxElem = existing->multiSub.maxElem;
	pThis->multiSub.nElem = 0;
	CHKmalloc(pThis->multiSub.ppMsgs = MALLOC(pThis->multiSub.maxElem * sizeof(smsg_t*)));
	pThis->iSeverity = existing->iSeverity;
	pThis->iFacility = existing->iFacility;
	pThis->maxLinesAtOnce = existing->maxLinesAtOnce;
	pThis->trimLineOverBytes = existing->trimLineOverBytes;
	pThis->iPersistStateInterval = existing->iPersistStateInterval;
	pThis->readMode = existing->readMode;
	pThis->startRegex = existing->startRegex; /* no strdup, as it is read-only */
	if(pThis->startRegex != NULL) // TODO: make this a single function with better error handling
		if(regcomp(&pThis->end_preg, (char*)pThis->startRegex, REG_EXTENDED)) {
			DBGPRINTF("imfile: error regex compile\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}
	pThis->bRMStateOnDel = existing->bRMStateOnDel;
	pThis->hasWildcard = existing->hasWildcard;
	pThis->escapeLF = existing->escapeLF;
	pThis->reopenOnTruncate = existing->reopenOnTruncate;
	pThis->addMetadata = existing->addMetadata;
	pThis->addCeeTag = existing->addCeeTag;
	pThis->readTimeout = existing->readTimeout;
	pThis->freshStartTail = existing->freshStartTail;
	pThis->pRuleset = existing->pRuleset;
	pThis->nRecords = 0;
	pThis->pStrm = NULL;
	pThis->prevLineSegment = NULL;
	pThis->masterLstn = existing;
	*ppExisting = pThis;
finalize_it:
	RETiRet;
}

/* Setup a new file watch for dynamically discovered files (via wildcards).
 * Note: we need to try to read this file, as it may already contain data this
 * needs to be processed, and we won't get an event for that as notifications
 * happen only for things after the watch has been activated.
 */
static void
in_setupFileWatchDynamic(lstn_t *pLstn, uchar *const __restrict__ newBaseName, uchar *const __restrict__ newFileName)
{
	char fullfn[MAXFNAME];
	struct stat fileInfo;
	uchar basedir[MAXFNAME];
	uchar* pBaseDir = NULL; 
	int idirindex; 

	if (newFileName == NULL) {
		/* Combine directory and filename */
		snprintf(fullfn, MAXFNAME, "%s/%s", pLstn->pszDirName, newBaseName);
	} else {
		/* Get BaseDir from filename! */
		getBasedir(basedir, newFileName);
		pBaseDir = &basedir[0]; /* Set BaseDir Pointer */
		idirindex = dirsFindDir(basedir); 
		if (idirindex == -1) {
			/* Add dir to table and create watch */ 
			DBGPRINTF("imfile: Adding new dir '%s' to dirs table \n", basedir); 
			dirsAdd(basedir, &idirindex);
			in_setupDirWatch(idirindex);
		}
		/* Use newFileName */
		snprintf(fullfn, MAXFNAME, "%s", newFileName);
	}
	if(stat(fullfn, &fileInfo) != 0) {
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		DBGPRINTF("imfile: ignoring file '%s' cannot stat(): %s\n",
			fullfn, errStr);
		goto done;
	}

	if(S_ISDIR(fileInfo.st_mode)) {
		DBGPRINTF("imfile: ignoring directory '%s'\n", fullfn);
		goto done;
	}

	if(lstnDup(&pLstn, newBaseName, pBaseDir) != RS_RET_OK)
		goto done;

	startLstnFile(pLstn);
done:	return;
}

/* Setup a new file watch for static (configured) files.
 * Note: we need to try to read this file, as it may already contain data this
 * needs to be processed, and we won't get an event for that as notifications
 * happen only for things after the watch has been activated.
 */
static void
in_setupFileWatchStatic(lstn_t *pLstn)
{
	DBGPRINTF("imfile: adding file '%s' to configured table\n",
		  pLstn->pszFileName);
	dirsAddFile(pLstn, CONFIGURED_FILE);

	if(pLstn->hasWildcard) {
		DBGPRINTF("imfile: file '%s' has wildcard, doing initial "
			  "expansion\n", pLstn->pszFileName);
		glob_t files;
		const int ret = glob((char*)pLstn->pszFileName,
					GLOB_MARK|GLOB_NOSORT|GLOB_BRACE, NULL, &files);
		if(ret == 0) {
			for(unsigned i = 0 ; i < files.gl_pathc ; i++) {
				uchar basen[MAXFNAME];
				uchar *const file = (uchar*)files.gl_pathv[i];

				if(file[strlen((char*)file)-1] == '/')
					continue;/* we cannot process subdirs! */

				getBasename(basen, file);
				DBGPRINTF("imfile: setup dynamic watch for '%s : %s' \n", basen, file);
				in_setupFileWatchDynamic(pLstn, basen, file);
			}
			globfree(&files);
		}
	} else {
		/* Duplicate static object as well, otherwise the configobject could be deleted later! */
		if(lstnDup(&pLstn, pLstn->pszBaseName, NULL) != RS_RET_OK) {
			DBGPRINTF("imfile: in_setupFileWatchStatic failed to duplicate listener for '%s'\n", pLstn->pszFileName);
			goto done;
		}
		startLstnFile(pLstn);
	}
done:	return;
}

/* setup our initial set of watches, based on user config */
static void
in_setupInitialWatches(void)
{
	int i;
	for(i = 0 ; i < currMaxDirs ; ++i) {
		in_setupDirWatch(i);
	}
	lstn_t *pLstn;
	for(pLstn = runModConf->pRootLstn ; pLstn != NULL ; pLstn = pLstn->next) {
		if(pLstn->masterLstn == NULL) {
			/* we process only static (master) entries */
			in_setupFileWatchStatic(pLstn);
		}
	}
}

static void
in_dbg_showEv(struct inotify_event *ev)
{
	if(ev->mask & IN_IGNORED) {
		DBGPRINTF("INOTIFY event: watch was REMOVED\n");
	} else if(ev->mask & IN_MODIFY) {
		DBGPRINTF("INOTIFY event: watch was MODIFID\n");
	} else if(ev->mask & IN_ACCESS) {
		DBGPRINTF("INOTIFY event: watch IN_ACCESS\n");
	} else if(ev->mask & IN_ATTRIB) {
		DBGPRINTF("INOTIFY event: watch IN_ATTRIB\n");
	} else if(ev->mask & IN_CLOSE_WRITE) {
		DBGPRINTF("INOTIFY event: watch IN_CLOSE_WRITE\n");
	} else if(ev->mask & IN_CLOSE_NOWRITE) {
		DBGPRINTF("INOTIFY event: watch IN_CLOSE_NOWRITE\n");
	} else if(ev->mask & IN_CREATE) {
		DBGPRINTF("INOTIFY event: file was CREATED: %s\n", ev->name);
	} else if(ev->mask & IN_DELETE) {
		DBGPRINTF("INOTIFY event: watch IN_DELETE\n");
	} else if(ev->mask & IN_DELETE_SELF) {
		DBGPRINTF("INOTIFY event: watch IN_DELETE_SELF\n");
	} else if(ev->mask & IN_MOVE_SELF) {
		DBGPRINTF("INOTIFY event: watch IN_MOVE_SELF\n");
	} else if(ev->mask & IN_MOVED_FROM) {
		DBGPRINTF("INOTIFY event: watch IN_MOVED_FROM\n");
	} else if(ev->mask & IN_MOVED_TO) {
		DBGPRINTF("INOTIFY event: watch IN_MOVED_TO\n");
	} else if(ev->mask & IN_OPEN) {
		DBGPRINTF("INOTIFY event: watch IN_OPEN\n");
	} else if(ev->mask & IN_ISDIR) {
		DBGPRINTF("INOTIFY event: watch IN_ISDIR\n");
	} else {
		DBGPRINTF("INOTIFY event: unknown mask code %8.8x\n", ev->mask);
	 }
}

/* 
*	Helper function to get fullpath when handling inotify dir events 
*/
static void 
in_handleDirGetFullDir(char* pszoutput, char* pszrootdir, char* pszsubdir)
{
	sbool hasWildcard;
	char dirnametrunc[MAXFNAME];
	int dirnamelen = 0;
	char* psztmp; 

	/* check for wildcard in directoryname, if last character is a wildcard we remove it and try again! */
	dirnamelen = ustrlen(pszrootdir); 
	memcpy(dirnametrunc, pszrootdir, dirnamelen); /* Copy mem */
	dirnametrunc[dirnamelen] = '\0'; /* Terminate copied string */

	hasWildcard = containsGlobWildcard(dirnametrunc);
	if(hasWildcard) {
		/* Set NULL Byte on last directory delimiter occurrence,
		will also remove asterix */
		psztmp = strrchr(dirnametrunc, '/');
		*psztmp = '\0';
	}

	/* Combine directory and new subdir */
	snprintf(pszoutput, MAXFNAME, "%s/%s", dirnametrunc, pszsubdir);
}

/* inotify told us that a file's wd was closed. We now need to remove
 * the file from our internal structures. Remember that a different inode
 * with the same name may already be in processing.
 */
static void
in_removeFile(const int dirIdx,
	      lstn_t *const __restrict__ pLstn)
{
	uchar statefile[MAXFNAME];
	uchar toDel[MAXFNAME];
	int bDoRMState;
	int wd;
	uchar *statefn;

	DBGPRINTF("imfile: remove listener '%s', dirIdx %d\n",
	          pLstn->pszFileName, dirIdx);
	if(pLstn->bRMStateOnDel) {
		statefn = getStateFileName(pLstn, statefile, sizeof(statefile));
		/* Get full path and file name */
		getFullStateFileName(statefn, toDel, sizeof(toDel)); 
		bDoRMState = 1;
	} else {
		bDoRMState = 0;
	}
	pollFile(pLstn, NULL); /* one final try to gather data */
	/*	delete listener data */
	DBGPRINTF("imfile: DELETING listener data for '%s' - '%s'\n", pLstn->pszBaseName, pLstn->pszFileName);
	lstnDel(pLstn);
	fileTableDelFile(&dirs[dirIdx].active, pLstn);
	if(bDoRMState) {
		DBGPRINTF("imfile: unlinking '%s'\n", toDel);
		if(unlink((char*)toDel) != 0) {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			errmsg.LogError(0, RS_RET_ERR, "imfile: could not remove state "
				"file \"%s\": %s", toDel, errStr);
		}
	}

	wd = wdmapLookupListner(pLstn);
	wdmapDel(wd);
}

static void
in_handleDirEventDirCREATE(struct inotify_event *ev, const int dirIdx)
{
	char fulldn[MAXFNAME];
	int newdiridx; 

	/* Combine to Full Path first */
	in_handleDirGetFullDir(fulldn, (char*)dirs[dirIdx].dirName, (char*)ev->name); 

	/* Search for existing entry first! */
	newdiridx = dirsFindDir( (uchar*)fulldn ); 
	if(newdiridx == -1) {
		/* Add dir to table and create watch */ 
		DBGPRINTF("imfile: Adding new dir '%s' to dirs table \n", fulldn); 
		dirsAdd((uchar*)fulldn, &newdiridx);
		in_setupDirWatch(newdiridx);
	} else {
		DBGPRINTF("imfile: dir '%s' already exists in dirs table (Idx %d)\n", fulldn, newdiridx); 
	}
}

static void
in_handleDirEventFileCREATE(struct inotify_event *ev, const int dirIdx)
{
	int i;
	lstn_t *pLstn;
	int ftIdx;
	char fullfn[MAXFNAME];
	uchar* pszDir = NULL; 
	int dirIdxFinal = dirIdx; 
	ftIdx = fileTableSearch(&dirs[dirIdxFinal].active, (uchar*)ev->name);
	if(ftIdx >= 0) {
		pLstn = dirs[dirIdxFinal].active.listeners[ftIdx].pLstn;
	} else {
/*		DBGPRINTF("imfile: file '%s' not active in dir '%s'\n",
			ev->name, dirs[dirIdxFinal].dirName);*/
		DBGPRINTF("imfile: in_handleDirEventFileCREATE  '%s' not associated with dir '%s' (CurMax:%d)\n", 
			ev->name, dirs[dirIdxFinal].dirName, dirs[dirIdxFinal].active.currMax);
		ftIdx = fileTableSearch(&dirs[dirIdxFinal].configured, (uchar*)ev->name);
		if(ftIdx == -1) {
			/* Search all other configured directories for proper index! */
			if (currMaxDirs > 0) {
				/* Store Dirname as we need to overwrite it in in_setupFileWatchDynamic */
				pszDir = dirs[dirIdxFinal].dirName; 

				/* Combine directory and filename */
				snprintf(fullfn, MAXFNAME, "%s/%s", pszDir, (uchar*)ev->name);

				for(i = 0 ; i < currMaxDirs ; ++i) {
					ftIdx = fileTableSearch(&dirs[i].configured, (uchar*)ev->name);
					if(ftIdx != -1) {
						/* Found matching directory! */
						dirIdxFinal = i; /* Have to correct directory index for listnr dupl 
											in in_setupFileWatchDynamic */
						break; 
					}
				}

				if(ftIdx == -1) {
					DBGPRINTF("imfile: file '%s' not associated with dir '%s' and also no " 
						"matching wildcard directory found\n", ev->name, dirs[dirIdxFinal].dirName);
					goto done;
				}
				else {
					DBGPRINTF("imfile: file '%s' not associated with dir '%s', using dirIndex %d instead\n",
						ev->name, (pszDir == NULL) ? dirs[dirIdxFinal].dirName : pszDir, dirIdxFinal);
				}
			} else {
				DBGPRINTF("imfile: file '%s' not associated with dir '%s'\n",
					ev->name, dirs[dirIdxFinal].dirName);
				goto done;
			}
		}
		pLstn = dirs[dirIdxFinal].configured.listeners[ftIdx].pLstn;
	}
	DBGPRINTF("imfile: file '%s' associated with dir '%s'\n", 
		ev->name, (pszDir == NULL) ? dirs[dirIdxFinal].dirName : pszDir);
	in_setupFileWatchDynamic(pLstn, (uchar*)ev->name, (pszDir == NULL) ? NULL : (uchar*)fullfn);
done:	return;
}

/* note: we need to care only for active files in the DELETE case.
 * Two reasons: a) if this is a configured file, it should be active
 * b) if not for some reason, there still is nothing we can do against
 * it, and trying to process a *deleted* file really makes no sense
 * (remeber we don't have it open, so it actually *is gone*).
 */
static void
in_handleDirEventFileDELETE(struct inotify_event *const ev, const int dirIdx)
{
	const int ftIdx = fileTableSearch(&dirs[dirIdx].active, (uchar*)ev->name);
	if(ftIdx == -1) {
		DBGPRINTF("imfile: deleted file '%s' not active in dir '%s'\n",
			ev->name, dirs[dirIdx].dirName);
		goto done;
	}
	DBGPRINTF("imfile: imfile delete processing for '%s'\n",
	          dirs[dirIdx].active.listeners[ftIdx].pLstn->pszFileName);
	in_removeFile(dirIdx, dirs[dirIdx].active.listeners[ftIdx].pLstn);
done:	return;
}

/* inotify told us that a dirs's wd was closed. We now need to remove
 * the dir from our internal structures. 
 */
static void 
in_removeDir(const int dirIdx)
{
	int wd;
	wd = wdmapLookupListnerIdx(dirIdx);
	DBGPRINTF("imfile: in_removeDir remove '%s', dirIdx=%d wdindex=%d\n", dirs[dirIdx].dirName, dirIdx, wd);
	wdmapDel(wd);
}

static void
in_handleDirEventDirDELETE(struct inotify_event *const ev, const int dirIdx)
{
	char fulldn[MAXFNAME];
	int finddiridx; 

	in_handleDirGetFullDir(fulldn, (char*)dirs[dirIdx].dirName, (char*)ev->name); 

	/* Search for existing entry first! */
	finddiridx = dirsFindDir( (uchar*)fulldn ); 

	if(finddiridx != -1) {
		/* Remove internal structures */
		in_removeDir(finddiridx);

		/* Delete dir from dirs array! */ 
		free(dirs[finddiridx].dirName);
		free(dirs[finddiridx].active.listeners);
		free(dirs[finddiridx].configured.listeners);
		dirs[finddiridx].dirName = NULL; 

		DBGPRINTF("imfile: in_handleDirEventDirDELETE dir (idx %d) '%s' deleted \n", finddiridx, fulldn); 
	} else {
		DBGPRINTF("imfile: in_handleDirEventDirDELETE ERROR could not found '%s' in dirs table!\n", fulldn); 
	}

/*
	const int ftIdx = fileTableSearch(&dirs[dirIdx].active, (uchar*)ev->name);
	if(ftIdx == -1) {
		DBGPRINTF("imfile: deleted file '%s' not active in dir '%s'\n",
			ev->name, dirs[dirIdx].dirName);
		goto done;
	}
	DBGPRINTF("imfile: imfile delete processing for '%s'\n",
	          dirs[dirIdx].active.listeners[ftIdx].pLstn->pszFileName);
	in_removeFile(dirIdx, dirs[dirIdx].active.listeners[ftIdx].pLstn);
*/
}

static void
in_handleDirEvent(struct inotify_event *const ev, const int dirIdx)
{
	DBGPRINTF("imfile: in_handleDirEvent dir event for (Idx %d)%s (mask %x)\n", dirIdx, dirs[dirIdx].dirName, ev->mask);
	if((ev->mask & IN_CREATE)) {
		if((ev->mask & IN_ISDIR))
			in_handleDirEventDirCREATE(ev, dirIdx); /* Create new Dir */
		else
			in_handleDirEventFileCREATE(ev, dirIdx); /* Create new File */
	} else if((ev->mask & IN_DELETE)) {
		if((ev->mask & IN_ISDIR))
			in_handleDirEventDirDELETE(ev, dirIdx);		/* Create new Dir */
		else
			in_handleDirEventFileDELETE(ev, dirIdx);	/* Delete File from dir filetable */
	} else {
		DBGPRINTF("imfile: got non-expected inotify event:\n");
		in_dbg_showEv(ev);
	}
}


static void
in_handleFileEvent(struct inotify_event *ev, const wd_map_t *const etry)
{
	if(ev->mask & IN_MODIFY) {
		pollFile(etry->pLstn, NULL);
	} else {
		DBGPRINTF("imfile: got non-expected inotify event:\n");
		in_dbg_showEv(ev);
	}
}

static void
in_processEvent(struct inotify_event *ev)
{
	wd_map_t *etry;
	lstn_t *pLstn;
	int iRet;
	int ftIdx;
	int wd;

	if(ev->mask & IN_IGNORED) {
		goto done;
	} else if(ev->mask & IN_MOVED_FROM) {
		/* Find wd entry and remove it */
		etry =  wdmapLookup(ev->wd);
		if(etry != NULL) {
			ftIdx = fileTableSearchNoWildcard(&dirs[etry->dirIdx].active, (uchar*)ev->name);
			DBGPRINTF("imfile: IN_MOVED_FROM Event (ftIdx=%d, name=%s)\n", ftIdx, ev->name);
			if(ftIdx >= 0) {
				/* Find listener and wd table index*/
				pLstn = dirs[etry->dirIdx].active.listeners[ftIdx].pLstn;
				wd = wdmapLookupListner(pLstn);

				/* Remove file from inotify watch */
				iRet = inotify_rm_watch(ino_fd, wd); /* Note this will TRIGGER IN_IGNORED Event! */
				if (iRet != 0) {
					DBGPRINTF("imfile: inotify_rm_watch error %d (ftIdx=%d, wd=%d, name=%s)\n", 
						errno, ftIdx, wd, ev->name);
				} else {
					DBGPRINTF("imfile: inotify_rm_watch successfully removed file from watch " 
						"(ftIdx=%d, wd=%d, name=%s)\n", ftIdx, wd, ev->name);
				}
				in_removeFile(etry->dirIdx, pLstn);
				DBGPRINTF("imfile: IN_MOVED_FROM Event file removed file (wd=%d, name=%s)\n", wd, ev->name);
			}
		}
		goto done;
	}
	DBGPRINTF("imfile: in_processEvent process Event %x for %s\n", ev->mask, (uchar*)ev->name);

	etry =  wdmapLookup(ev->wd);
	if(etry == NULL) {
		DBGPRINTF("imfile: could not lookup wd %d\n", ev->wd);
		goto done;
	}
	if(etry->pLstn == NULL) { /* directory? */
		in_handleDirEvent(ev, etry->dirIdx);
	} else {
		in_handleFileEvent(ev, etry);
	}
done:	return;
}

static void
in_do_timeout_processing(void)
{
	int i;
	DBGPRINTF("imfile: readTimeouts are configured, checking if some apply\n");

	for(i = 0 ; i < nWdmap ; ++i) {
		dbgprintf("imfile: wdmap %d, plstn %p\n", i, wdmap[i].pLstn);
		lstn_t *const pLstn = wdmap[i].pLstn;
		if(pLstn != NULL && strmReadMultiLine_isTimedOut(pLstn->pStrm)) {
			dbgprintf("imfile: wdmap %d, timeout occured\n", i);
			pollFile(pLstn, NULL);
		}
	}

}


/* Monitor files in inotify mode */
#if !defined(_AIX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align" /* TODO: how can we fix these warnings? */
#endif
/* Problem with the warnings: they seem to stem back from the way the API is structured */
static rsRetVal
do_inotify(void)
{
	char iobuf[8192];
	struct inotify_event *ev;
	int rd;
	int currev;
	DEFiRet;

	CHKiRet(wdmapInit());
	CHKiRet(dirsInit());
	ino_fd = inotify_init();
	if(ino_fd < 0) {
		errmsg.LogError(1, RS_RET_INOTIFY_INIT_FAILED, "imfile: Init inotify "
			"instance failed ");
		return RS_RET_INOTIFY_INIT_FAILED;
	}
	DBGPRINTF("imfile: inotify fd %d\n", ino_fd);
	in_setupInitialWatches();

	while(glbl.GetGlobalInputTermState() == 0) {
		if(runModConf->haveReadTimeouts) {
			int r;
			struct pollfd pollfd;
			pollfd.fd = ino_fd;
			pollfd.events = POLLIN;
			do {
				r = poll(&pollfd, 1, runModConf->timeoutGranularity);
			} while(r  == -1 && errno == EINTR);
			if(r == 0) {
				in_do_timeout_processing();
				continue;
			} else if (r == -1) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				DBGPRINTF("%s:%d: unexpected error during poll timeout wait: %s\n",
					__FILE__, __LINE__, errStr);
				ABORT_FINALIZE(RS_RET_IO_ERROR);
			} else if(r != 1) {
				DBGPRINTF("%s:%d: ERROR: poll returned %d, but we had only one fd!\n",
					__FILE__, __LINE__, r);
				ABORT_FINALIZE(RS_RET_IO_ERROR);
			}
		}
		rd = read(ino_fd, iobuf, sizeof(iobuf));
		if(rd < 0 && Debug) {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("imfile: error during inotify: %s\n", errStr);
		}
		currev = 0;
		while(currev < rd) {
			ev = (struct inotify_event*) (iobuf+currev);
			in_dbg_showEv(ev);
			in_processEvent(ev);
			currev += sizeof(struct inotify_event) + ev->len;
		}
	}

finalize_it:
	close(ino_fd);
	RETiRet;
}
#pragma GCC diagnostic pop

#else /* #if HAVE_INOTIFY_INIT */
static rsRetVal
do_inotify(void)
{
	errmsg.LogError(0, RS_RET_NOT_IMPLEMENTED, "imfile: mode set to inotify, but the "
			"platform does not support inotify");
	return RS_RET_NOT_IMPLEMENTED;
}
#endif /* #if HAVE_INOTIFY_INIT */

/* This function is called by the framework to gather the input. The module stays
 * most of its lifetime inside this function. It MUST NEVER exit this function. Doing
 * so would end module processing and rsyslog would NOT reschedule the module. If
 * you exit from this function, you violate the interface specification!
 */
BEGINrunInput
CODESTARTrunInput
	DBGPRINTF("imfile: working in %s mode\n",
		 (runModConf->opMode == OPMODE_POLLING) ? "polling" : "inotify");
	if(runModConf->opMode == OPMODE_POLLING)
		iRet = doPolling();
	else
		iRet = do_inotify();

	DBGPRINTF("imfile: terminating upon request of rsyslog core\n");
ENDrunInput


/* The function is called by rsyslog before runInput() is called. It is a last chance
 * to set up anything specific. Most importantly, it can be used to tell rsyslog if the
 * input shall run or not. The idea is that if some config settings (or similiar things)
 * are not OK, the input can tell rsyslog it will not execute. To do so, return
 * RS_RET_NO_RUN or a specific error code. If RS_RET_OK is returned, rsyslog will
 * proceed and call the runInput() entry point.
 */
BEGINwillRun
CODESTARTwillRun
	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imfile"), sizeof("imfile") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

finalize_it:
ENDwillRun

/* This function persists information for a specific file being monitored.
 * To do so, it simply persists the stream object. We do NOT abort on error
 * iRet as that makes matters worse (at least we can try persisting the others...).
 * rgerhards, 2008-02-13
 */
static rsRetVal
persistStrmState(lstn_t *pLstn)
{
	DEFiRet;
	strm_t *psSF = NULL; /* state file (stream) */
	size_t lenDir;
	uchar statefile[MAXFNAME];

	uchar *const statefn = getStateFileName(pLstn, statefile, sizeof(statefile));
	DBGPRINTF("imfile: persisting state for '%s' to file '%s'\n",
		  pLstn->pszFileName, statefn);
	CHKiRet(strm.Construct(&psSF));
	lenDir = ustrlen(glbl.GetWorkDir());
	if(lenDir > 0)
		CHKiRet(strm.SetDir(psSF, glbl.GetWorkDir(), lenDir));
	CHKiRet(strm.SettOperationsMode(psSF, STREAMMODE_WRITE_TRUNC));
	CHKiRet(strm.SetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetFName(psSF, statefn, strlen((char*) statefn)));
	CHKiRet(strm.ConstructFinalize(psSF));

	CHKiRet(strm.Serialize(pLstn->pStrm, psSF));
	CHKiRet(strm.Flush(psSF));

	CHKiRet(strm.Destruct(&psSF));

finalize_it:
	if(psSF != NULL)
		strm.Destruct(&psSF);

	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "imfile: could not persist state "
				"file %s - data may be repeated on next "
				"startup. Is WorkDirectory set?",
				statefn);
	}

	RETiRet;
}

/* This function is called by the framework after runInput() has been terminated. It
 * shall free any resources and prepare the module for unload.
 */
BEGINafterRun
CODESTARTafterRun
	while(runModConf->pRootLstn != NULL) {
		/* Note: lstnDel() reasociates root! */
		lstnDel(runModConf->pRootLstn);
	}

	if(pInputName != NULL)
		prop.Destruct(&pInputName);
ENDafterRun


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


/* The following entry points are defined in module-template.h.
 * In general, they need to be present, but you do NOT need to provide
 * any code here.
 */
BEGINmodExit
CODESTARTmodExit
	int i;
	/* release objects we used */
	objRelease(strm, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
#ifdef HAVE_INOTIFY_INIT
	/* we use these vars only in inotify mode */
	if(dirs != NULL) {
		/* Free dirNames */
		for(i = 0 ; i < currMaxDirs ; ++i)
			free(dirs[i].dirName); 
		free(dirs->active.listeners);
		free(dirs->configured.listeners);
		free(dirs);
	}
	free(wdmap);
#endif
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt


/* The following function shall reset all configuration variables to their
 * default values. The code provided in modInit() below registers it to be
 * called on "$ResetConfigVariables". You may also call it from other places,
 * but in general this is not necessary. Once runInput() has been called, this
 * function here is never again called.
 */
static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;

	free(cs.pszFileName);
	cs.pszFileName = NULL;
	free(cs.pszFileTag);
	cs.pszFileTag = NULL;
	free(cs.pszStateFile);
	cs.pszStateFile = NULL;

	/* set defaults... */
	cs.iPollInterval = DFLT_PollInterval;
	cs.iFacility = 128; /* local0 */
	cs.iSeverity = 5;  /* notice, as of rfc 3164 */
	cs.readMode = 0;
	cs.maxLinesAtOnce = 10240;
	cs.trimLineOverBytes = 0;

	RETiRet;
}

static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	errmsg.LogError(0, NO_ERRCODE, "imfile: ruleset '%s' for %s not found - "
			"using default ruleset instead", inst->pszBindRuleset,
			inst->pszFileName);
}

/* modInit() is called once the module is loaded. It must perform all module-wide
 * initialization tasks. There are also a number of housekeeping tasks that the
 * framework requires. These are handled by the macros. Please note that the
 * complexity of processing is depending on the actual module. However, only
 * thing absolutely necessary should be done here. Actual app-level processing
 * is to be performed in runInput(). A good sample of what to do here may be to
 * set some variable defaults.
 */
BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(strm, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));

	DBGPRINTF("imfile: version %s initializing\n", VERSION);
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilename", 0, eCmdHdlrGetWord,
	  	NULL, &cs.pszFileName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfiletag", 0, eCmdHdlrGetWord,
	  	NULL, &cs.pszFileTag, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilestatefile", 0, eCmdHdlrGetWord,
	  	NULL, &cs.pszStateFile, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfileseverity", 0, eCmdHdlrSeverity,
	  	NULL, &cs.iSeverity, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilefacility", 0, eCmdHdlrFacility,
	  	NULL, &cs.iFacility, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilereadmode", 0, eCmdHdlrInt,
	  	NULL, &cs.readMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilemaxlinesatonce", 0, eCmdHdlrSize,
	  	NULL, &cs.maxLinesAtOnce, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfiletrimlineoverbytes", 0, eCmdHdlrSize,
	  	NULL, &cs.trimLineOverBytes, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilepersiststateinterval", 0, eCmdHdlrInt,
	  	NULL, &cs.iPersistStateInterval, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilebindruleset", 0, eCmdHdlrGetWord,
		NULL, &cs.pszBindRuleset, STD_LOADABLE_MODULE_ID));
	/* that command ads a new file! */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputrunfilemonitor", 0, eCmdHdlrGetWord,
		addInstance, NULL, STD_LOADABLE_MODULE_ID));
	/* module-global config params - will be disabled in configs that are loaded
	 * via module(...).
	 */
	CHKiRet(regCfSysLineHdlr2((uchar *)"inputfilepollinterval", 0, eCmdHdlrInt,
	  	NULL, &cs.iPollInterval, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
