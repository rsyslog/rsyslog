/* imfile.c
 *
 * This is the input module for reading text file data. A text file is a
 * non-binary file who's lines are delemited by the \n character.
 *
 * Work originally begun on 2008-02-01 by Rainer Gerhards
 *
 * Copyright 2008-2017 Adiscon GmbH.
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
#include <linux/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif
#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
#include <port.h>
#include <sys/port.h>
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

/*
*	Helpers for wildcard in directory detection
*/
#define DIR_CONFIGURED 0
#define DIR_DYNAMIC 1

/* If set to 1, fileTableDisplay will be compiled and used for debugging */
#define ULTRA_DEBUG 0

/* Setting GLOB_BRACE to ZERO which disables support for GLOB_BRACE if not available on current platform */
#ifndef GLOB_BRACE
	#define GLOB_BRACE 0
#endif

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
	uint8_t readMode;	/* which mode to use in ReadMultiLine call? */
	uchar *startRegex;	/* regex that signifies end of message (NULL if unset) */
	sbool discardTruncatedMsg;
	sbool msgDiscardingError;
	regex_t end_preg;	/* compiled version of startRegex */
	uchar *prevLineSegment;	/* previous line segment (in regex mode) */
	sbool escapeLF;	/* escape LF inside the MSG content? */
	sbool reopenOnTruncate;
	sbool addMetadata;
	sbool addCeeTag;
	sbool freshStartTail; /* read from tail of file on fresh start? */
	sbool fileNotFoundError;
	ruleset_t *pRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	ratelimit_t *ratelimiter;
	multi_submit_t multiSub;
#ifdef HAVE_INOTIFY_INIT
	uchar* movedfrom_statefile;
	__u32 movedfrom_cookie;
#endif
#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
	struct fileinfo *pfinf;
	sbool bPortAssociated;
#endif
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
	sbool discardTruncatedMsg;
	sbool msgDiscardingError;
	sbool escapeLF;
	sbool reopenOnTruncate;
	sbool addCeeTag;
	sbool addMetadata;
	sbool freshStartTail;
	sbool fileNotFoundError;
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
#define OPMODE_FEN 2

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
	sbool sortFiles;
	sbool haveReadTimeouts;	/* use special processing if read timeouts exist */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */


/* Dynamic File support for inotify / fen mode */
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
	uchar *dirNameBfWildCard;
	sbool hasWildcard;
/* TODO: check if following property are inotify only?*/
	int bDirType;		/* Configured or dynamic */
	fileTable_t active;	/* associated active files */
	fileTable_t configured; /* associated configured files */
	int rdiridx;		/* Root diridx helper property */
#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
	struct fileinfo *pfinf;
	sbool bPortAssociated;
#endif
};

#if defined(HAVE_INOTIFY_INIT) || (defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE))
/* needed for inotify / fen mode */
typedef struct dirInfo_s dirInfo_t;
static dirInfo_t *dirs = NULL;
static int allocMaxDirs;
static int currMaxDirs;
#endif /* #if defined(HAVE_INOTIFY_INIT) || (defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)) --- */

/* the following two macros are used to select the correct file table */
#define ACTIVE_FILE 1
#define CONFIGURED_FILE 0

#ifdef HAVE_INOTIFY_INIT
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

#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
struct fileinfo {
	struct file_obj fobj;
	int events;
	int port;
};

static int glport; /* Static port handle for FEN api*/

/* Need these function to be declared on top */
static rsRetVal fen_DirSearchFiles(lstn_t *pLstn, int dirIdx);
static rsRetVal fen_processEventDir(struct file_obj* fobjp, int dirIdx, int revents);
#endif /* #if OS_SOLARIS -------------------------------------------------- */

static prop_t *pInputName = NULL;
/* there is only one global inputName for all messages generated by this input */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "pollinginterval", eCmdHdlrPositiveInt, 0 },
	{ "readtimeout", eCmdHdlrPositiveInt, 0 },
	{ "timeoutgranularity", eCmdHdlrPositiveInt, 0 },
	{ "sortfiles", eCmdHdlrBinary, 0 },
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
	{ "discardtruncatedmsg", eCmdHdlrBinary, 0 },
	{ "msgdiscardingerror", eCmdHdlrBinary, 0 },
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
	{ "freshstarttail", eCmdHdlrBinary, 0},
	{ "filenotfounderror", eCmdHdlrBinary, 0}
};
static struct cnfparamblk inppblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(inppdescr)/sizeof(struct cnfparamdescr),
	  inppdescr
	};

#include "im-helper.h" /* must be included AFTER the type definitions! */


#ifdef HAVE_INOTIFY_INIT
/* support for inotify mode */


#if ULTRA_DEBUG == 1
/* if ultra debugging enabled */
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

/* looks up a wdmap entry by filename and returns it's index if found
 * or -1 if not found.
 */
static int
wdmapLookupFilename(uchar *pszFileName)
{
	int i = 0;
	int wd = -1;
	/* Loop through */
	for(i = 0 ; i < nWdmap; ++i) {
		if (	wdmap[i].pLstn != NULL &&
			strcmp((const char*)wdmap[i].pLstn->pszFileName, (const char*)pszFileName) == 0){
			/* Found matching wd */
			wd = wdmap[i].wd;
		}
	}

	return wd;
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
		DBGPRINTF("wd %d already in wdmap!\n", wd);
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
	DBGPRINTF("enter into wdmap[%d]: wd %d, dir %d, lstn %s:%s\n",i,wd,dirIdx,
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
		DBGPRINTF("wd %d shall be deleted but not in wdmap!\n", wd);
		FINALIZE;
	}

	if(i < nWdmap-1) {
		/* we need to shift to delete it (see comment at wdmap definition) */
		memmove(wdmap + i, wdmap + i + 1, sizeof(wd_map_t) * (nWdmap - i - 1));
	}
	--nWdmap;
	DBGPRINTF("wd %d deleted, was idx %d\n", wd, i);

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
static uchar * ATTR_NONNULL(2)
getStateFileName(lstn_t *const __restrict__ pLstn,
	 	 uchar *const __restrict__ buf,
		 const size_t lenbuf,
		 const uchar *pszFileName)
{
	uchar *ret;

	/* Use pszFileName parameter if set */
	pszFileName = pszFileName == NULL ? pLstn->pszFileName : pszFileName;

	DBGPRINTF("getStateFileName for '%s'\n", pszFileName);
	if(pLstn == NULL || pLstn->pszStateFile == NULL) {
		snprintf((char*)buf, lenbuf - 1, "imfile-state:%s", pszFileName);
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
 * not freed - this must be done by the caller.
 */
#define MAX_OFFSET_REPRESENTATION_NUM_BYTES 20
static rsRetVal enqLine(lstn_t *const __restrict__ pLstn,
			cstr_t *const __restrict__ cstrLine,
			const int64 strtOffs)
{
	DEFiRet;
	smsg_t *pMsg;
	uchar file_offset[MAX_OFFSET_REPRESENTATION_NUM_BYTES+1];
	const uchar *metadata_names[2] = {(uchar *)"filename",(uchar *)"fileoffset"} ;
	const uchar *metadata_values[2] ;
	const size_t msgLen = cstrLen(cstrLine);

	if(msgLen == 0) {
		/* we do not process empty lines */
		FINALIZE;
	}

	CHKiRet(msgConstruct(&pMsg));
	MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
	MsgSetInputName(pMsg, pInputName);
	if(pLstn->addCeeTag) {
		/* Make sure we account for terminating null byte */
		size_t ceeMsgSize = msgLen + CONST_LEN_CEE_COOKIE + 1;
		char *ceeMsg;
		CHKmalloc(ceeMsg = MALLOC(ceeMsgSize));
		strcpy(ceeMsg, CONST_CEE_COOKIE);
		strcat(ceeMsg, (char*)rsCStrGetSzStrNoNULL(cstrLine));
		MsgSetRawMsg(pMsg, ceeMsg, ceeMsgSize);
		free(ceeMsg);
	} else {
		MsgSetRawMsg(pMsg, (char*)rsCStrGetSzStrNoNULL(cstrLine), msgLen);
	}
	MsgSetMSGoffs(pMsg, 0);	/* we do not have a header... */
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetTAG(pMsg, pLstn->pszTag, pLstn->lenTag);
	msgSetPRI(pMsg, pLstn->iFacility | pLstn->iSeverity);
	MsgSetRuleset(pMsg, pLstn->pRuleset);
	if(pLstn->addMetadata) {
		metadata_values[0] = pLstn->pszFileName;
		snprintf((char *)file_offset, MAX_OFFSET_REPRESENTATION_NUM_BYTES+1, "%lld", strtOffs);
		metadata_values[1] = file_offset;
		msgAddMultiMetadata(pMsg, metadata_names, metadata_values, 2);
	}
	ratelimitAddMsg(pLstn->ratelimiter, &pLstn->multiSub, pMsg);
finalize_it:
	RETiRet;
}


/* try to open a file which has a state file. If the state file does not
 * exist or cannot be read, an error is returned.
 */
static rsRetVal ATTR_NONNULL(1)
openFileWithStateFile(lstn_t *const __restrict__ pLstn)
{
	DEFiRet;
	strm_t *psSF = NULL;
	uchar pszSFNam[MAXFNAME];
	size_t lenSFNam;
	struct stat stat_buf;
	uchar statefile[MAXFNAME];

	uchar *const statefn = getStateFileName(pLstn, statefile, sizeof(statefile), NULL);
	DBGPRINTF("trying to open state for '%s', state file '%s'\n",
		  pLstn->pszFileName, statefn);

	/* Get full path and file name */
	lenSFNam = getFullStateFileName(statefn, pszSFNam, sizeof(pszSFNam));

	/* check if the file exists */
	if(stat((char*) pszSFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			DBGPRINTF("NO state file (%s) exists for '%s'\n", pszSFNam, pLstn->pszFileName);
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("error trying to access state file for '%s':%s\n",
			          pLstn->pszFileName, errStr);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}

	/* If we reach this point, we have a state file */

	CHKiRet(strm.Construct(&psSF));
	CHKiRet(strm.SettOperationsMode(psSF, STREAMMODE_READ));
	CHKiRet(strm.SetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetFName(psSF, pszSFNam, lenSFNam));
	CHKiRet(strm.SetFileNotFoundError(psSF, pLstn->fileNotFoundError));
	CHKiRet(strm.ConstructFinalize(psSF));

	/* read back in the object */
	CHKiRet(obj.Deserialize(&pLstn->pStrm, (uchar*) "strm", psSF, NULL, pLstn));
	DBGPRINTF("deserialized state file, state file base name '%s', "
		  "configured base name '%s'\n", pLstn->pStrm->pszFName,
		  pLstn->pszFileName);
	if(ustrcmp(pLstn->pStrm->pszFName, pLstn->pszFileName)) {
		if (pLstn->masterLstn != NULL) {
			/* StateFile was migrated from a filemove.
				We need to correct the stream Filename and continue */
			CHKmalloc(pLstn->pStrm->pszFName = ustrdup(pLstn->pszFileName));
			DBGPRINTF("statefile was migrated from a file rename for '%s'\n", pLstn->pszFileName);
		} else {
			LogError(0, RS_RET_STATEFILE_WRONG_FNAME, "imfile: state file '%s' "
					"contains file name '%s', but is used for file '%s'. State "
					"file deleted, starting from begin of file.",
					pszSFNam, pLstn->pStrm->pszFName, pLstn->pszFileName);

			unlink((char*)pszSFNam);
			ABORT_FINALIZE(RS_RET_STATEFILE_WRONG_FNAME);
		}
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

	DBGPRINTF("clean startup withOUT state file for '%s'\n", pLstn->pszFileName);
	if(pLstn->pStrm != NULL)
		strm.Destruct(&pLstn->pStrm);
	CHKiRet(strm.Construct(&pLstn->pStrm));
	CHKiRet(strm.SettOperationsMode(pLstn->pStrm, STREAMMODE_READ));
	CHKiRet(strm.SetsType(pLstn->pStrm, STREAMTYPE_FILE_MONITOR));
	CHKiRet(strm.SetFName(pLstn->pStrm, pLstn->pszFileName, strlen((char*) pLstn->pszFileName)));
	CHKiRet(strm.SetFileNotFoundError(pLstn->pStrm, pLstn->fileNotFoundError));
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

finalize_it:
	RETiRet;
}

/* try to open a file. This involves checking if there is a status file and,
 * if so, reading it in. Processing continues from the last known location.
 */
static rsRetVal
openFile(lstn_t *const __restrict__ pLstn)
{
	DEFiRet;

	CHKiRet_Hdlr(openFileWithStateFile(pLstn)) {
		CHKiRet(openFileWithoutStateFile(pLstn));
	}

	DBGPRINTF("breopenOnTruncate %d for '%s'\n",
		pLstn->reopenOnTruncate, pLstn->pszFileName);
	CHKiRet(strm.SetbReopenOnTruncate(pLstn->pStrm, pLstn->reopenOnTruncate));
	strmSetReadTimeout(pLstn->pStrm, pLstn->readTimeout);

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


/* pollFile needs to be split due to the unfortunate pthread_cancel_push() macros. */
static rsRetVal ATTR_NONNULL(1, 3)
pollFileReal(lstn_t *pLstn, int *pbHadFileData, cstr_t **pCStr)
{
	int64 strtOffs;
	DEFiRet;

	int nProcessed = 0;
	if(pLstn->pStrm == NULL) {
		CHKiRet(openFile(pLstn)); /* open file */
	}

	/* loop below will be exited when strmReadLine() returns EOF */
	while(glbl.GetGlobalInputTermState() == 0) {
		if(pLstn->maxLinesAtOnce != 0 && nProcessed >= pLstn->maxLinesAtOnce)
			break;
		if(pLstn->startRegex == NULL) {
			CHKiRet(strm.ReadLine(pLstn->pStrm, pCStr, pLstn->readMode, pLstn->escapeLF,
				pLstn->trimLineOverBytes, &strtOffs));
		} else {
			CHKiRet(strmReadMultiLine(pLstn->pStrm, pCStr, &pLstn->end_preg,
				pLstn->escapeLF, pLstn->discardTruncatedMsg, pLstn->msgDiscardingError, &strtOffs));
		}
		++nProcessed;
		if(pbHadFileData != NULL)
			*pbHadFileData = 1; /* this is just a flag, so set it and forget it */
		CHKiRet(enqLine(pLstn, *pCStr, strtOffs)); /* process line */
		rsCStrDestruct(pCStr); /* discard string (must be done by us!) */
		if(pLstn->iPersistStateInterval > 0 && ++pLstn->nRecords >= pLstn->iPersistStateInterval) {
			persistStrmState(pLstn);
			pLstn->nRecords = 0;
		}
	}

finalize_it:
	multiSubmitFlush(&pLstn->multiSub);

	if(*pCStr != NULL) {
		rsCStrDestruct(pCStr);
	}

	RETiRet;
}

/* poll a file, need to check file rollover etc. open file if not open */
static rsRetVal ATTR_NONNULL(1)
pollFile(lstn_t *pLstn, int *pbHadFileData)
{
	cstr_t *pCStr = NULL;
	DEFiRet;
	/* Note: we must do pthread_cleanup_push() immediately, because the POSIX macros
	 * otherwise do not work if I include the _cleanup_pop() inside an if... -- rgerhards, 2008-08-14
	 */
	pthread_cleanup_push(pollFileCancelCleanup, &pCStr);
	iRet = pollFileReal(pLstn, pbHadFileData, &pCStr);
	pthread_cleanup_pop(0);
	RETiRet;
}


/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal ATTR_NONNULL(1)
createInstance(instanceConf_t **const pinst)
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
	inst->discardTruncatedMsg = 0;
	inst->msgDiscardingError = 1;
	inst->bRMStateOnDel = 1;
	inst->escapeLF = 1;
	inst->reopenOnTruncate = 0;
	inst->addMetadata = ADD_METADATA_UNSPECIFIED;
	inst->addCeeTag = 0;
	inst->freshStartTail = 0;
	inst->fileNotFoundError = 1;
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
static int ATTR_NONNULL()
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

/* this function checks instance parameters and does some required pre-processing
 * (e.g. split filename in path and actual name)
 * Note: we do NOT use dirname()/basename() as they have portability problems.
 */
static rsRetVal ATTR_NONNULL()
checkInstance(instanceConf_t *inst)
{
	char dirn[MAXFNAME];
	uchar basen[MAXFNAME];
	int i;
	struct stat sb;
	int r;
	sbool hasWildcard;
	DEFiRet;

	/* this is primarily for the clang static analyzer, but also
	 * guards against logic errors in the config handler.
	 */
	if(inst->pszFileName == NULL)
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);

	i = getBasename(basen, inst->pszFileName);
	if (i == -1) {
		LogError(0, RS_RET_CONFIG_ERROR, "imfile: file path '%s' does not include a "
			"basename component", inst->pszFileName);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	memcpy(dirn, inst->pszFileName, i); /* do not copy slash */
	dirn[i] = '\0';

	DBGPRINTF("checking instance for directory [%s]\n", dirn);

	CHKmalloc(inst->pszFileBaseName = ustrdup(basen));
	CHKmalloc(inst->pszDirName = ustrdup(dirn));

	if(dirn[0] == '\0') {
		dirn[0] = '/';
		dirn[1] = '\0';
	}

	/* Check for Wildcards in FileBaseName.
	* If there is one, we need to truncate before the wildcard in order
	* to proceed further tests.
	*/
	hasWildcard = containsGlobWildcard(dirn);
	if(hasWildcard) {
		DBGPRINTF("wildcard in dirname, do not check if dir exists for [%s]\n", dirn);
		FINALIZE
	}

	r = stat(dirn, &sb);
	if(r != 0)  {
		LogError(errno, RS_RET_CONFIG_ERROR, "imfile warning: directory '%s'", dirn);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(!S_ISDIR(sb.st_mode)) {
		LogError(0, RS_RET_CONFIG_ERROR, "imfile warning: configured directory "
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
		LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no file name given, file monitor can "
					"not be created");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(cs.pszFileTag == NULL) {
		LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no tag value given, file monitor can "
					"not be created");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	CHKiRet(createInstance(&inst));
	if((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
		inst->pszBindRuleset = NULL;
	} else {
		CHKmalloc(inst->pszBindRuleset = ustrdup(cs.pszBindRuleset));
	}
	CHKmalloc(inst->pszFileName = ustrdup((char*) cs.pszFileName));
	CHKmalloc(inst->pszTag = ustrdup((char*) cs.pszFileTag));
	if(cs.pszStateFile == NULL) {
		inst->pszStateFile = NULL;
	} else {
		CHKmalloc(inst->pszStateFile = ustrdup(cs.pszStateFile));
	}
	inst->iSeverity = cs.iSeverity;
	inst->iFacility = cs.iFacility;
	if(cs.maxLinesAtOnce) {
		if(loadModConf->opMode == OPMODE_INOTIFY) {
			LogError(0, RS_RET_PARAM_NOT_PERMITTED,
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
static rsRetVal ATTR_NONNULL()
lstnAdd(lstn_t **const newLstn)
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
static void ATTR_NONNULL(1)
lstnDel(lstn_t *pLstn)
{
	DBGPRINTF("lstnDel called for %s\n", pLstn->pszFileName);
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
#ifdef HAVE_INOTIFY_INIT
	if (pLstn->movedfrom_statefile != NULL)
		free(pLstn->movedfrom_statefile);
#endif
#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
	if (pLstn->pfinf != NULL) {
		free(pLstn->pfinf->fobj.fo_name);
		free(pLstn->pfinf);
	}
#endif
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
static rsRetVal ATTR_NONNULL(1)
addListner(instanceConf_t *const inst)
{
	DEFiRet;
	lstn_t *pThis;
	sbool hasWildcard;

	hasWildcard = containsGlobWildcard((char*)inst->pszFileBaseName);
	DBGPRINTF("addListner file '%s', wildcard detected: %s\n",
		  inst->pszFileBaseName, (hasWildcard ? "TRUE" : "FALSE"));

	if(hasWildcard) {
		if(runModConf->opMode == OPMODE_POLLING) {
			LogError(0, RS_RET_IMFILE_WILDCARD,
				"imfile: The to-be-monitored file \"%s\" contains "
				"wildcards. This is not supported in "
				"polling mode.", inst->pszFileName);
			ABORT_FINALIZE(RS_RET_IMFILE_WILDCARD);
		} else if(inst->pszStateFile != NULL) {
			LogError(0, RS_RET_IMFILE_WILDCARD,
				"imfile: warning: it looks like to-be-monitored "
				"file \"%s\" contains wildcards. This usually "
				"does not work well with specifying a state file.",
				inst->pszFileName);
		}
	}

	CHKiRet(lstnAdd(&pThis));
	pThis->hasWildcard = hasWildcard;
	pThis->pszDirName = inst->pszDirName;
	CHKmalloc(pThis->pszFileName = ustrdup(inst->pszFileName));
	CHKmalloc(pThis->pszBaseName = ustrdup(inst->pszFileBaseName)); /* be consistent with expanded wildcards! */
	CHKmalloc(pThis->pszTag = ustrdup(inst->pszTag));
	pThis->lenTag = ustrlen(pThis->pszTag);
	if(inst->pszStateFile == NULL) {
		pThis->pszStateFile = NULL;
	} else {
		pThis->pszStateFile = ustrdup(inst->pszStateFile);
	}

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
	if(pThis->startRegex != NULL) {
		const int errcode = regcomp(&pThis->end_preg, (char*)pThis->startRegex, REG_EXTENDED);
		if(errcode != 0) {
			char errbuff[512];
			regerror(errcode, &pThis->end_preg, errbuff, sizeof(errbuff));
			LogError(0, NO_ERRCODE, "imfile: %s\n", errbuff);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}
	pThis->discardTruncatedMsg = inst->discardTruncatedMsg;
	pThis->msgDiscardingError = inst->msgDiscardingError;
	pThis->bRMStateOnDel = inst->bRMStateOnDel;
	pThis->escapeLF = inst->escapeLF;
	pThis->reopenOnTruncate = inst->reopenOnTruncate;
	pThis->addMetadata = (inst->addMetadata == ADD_METADATA_UNSPECIFIED) ?
			       hasWildcard : inst->addMetadata;
	pThis->addCeeTag = inst->addCeeTag;
	pThis->readTimeout = inst->readTimeout;
	pThis->freshStartTail = inst->freshStartTail;
	pThis->fileNotFoundError = inst->fileNotFoundError;
	pThis->pRuleset = inst->pBindRuleset;
	pThis->nRecords = 0;
	pThis->pStrm = NULL;
	pThis->prevLineSegment = NULL;
	pThis->masterLstn = NULL; /* we *are* a master! */
	#ifdef HAVE_INOTIFY_INIT
		/* Init Moved Files variables (Used for MOVED_TO/MOVED_FROM)*/
		pThis->movedfrom_statefile = NULL;
		pThis->movedfrom_cookie = 0;
	#endif
	#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
		pThis->pfinf = NULL;
		pThis->bPortAssociated = 0;
	#endif

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
		} else if(!strcmp(inppblk.descr[i].name, "discardtruncatedmsg")) {
			inst->discardTruncatedMsg = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "msgdiscardingerror")) {
			inst->msgDiscardingError = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "deletestateonfiledelete")) {
			inst->bRMStateOnDel = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "addmetadata")) {
			inst->addMetadata = (sbool) pvals[i].val.d.n;
		} else if (!strcmp(inppblk.descr[i].name, "addceetag")) {
			inst->addCeeTag = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "freshstarttail")) {
			inst->freshStartTail = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "filenotfounderror")) {
			inst->fileNotFoundError = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "escapelf")) {
			inst->escapeLF = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "reopenontruncate")) {
			inst->reopenOnTruncate = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "maxlinesatonce")) {
			if(   loadModConf->opMode == OPMODE_INOTIFY
			   && pvals[i].val.d.n > 0) {
				LogError(0, RS_RET_PARAM_NOT_PERMITTED,
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
			DBGPRINTF("program error, non-handled "
			  "param '%s'\n", inppblk.descr[i].name);
		}
	}
	if(inst->readMode != 0 &&  inst->startRegex != NULL) {
		LogError(0, RS_RET_PARAM_NOT_PERMITTED,
			"readMode and startmsg.regex cannot be set "
			"at the same time --- remove one of them");
			ABORT_FINALIZE(RS_RET_PARAM_NOT_PERMITTED);
	}
	if(inst->readTimeout != 0)
		loadModConf->haveReadTimeouts = 1;
	iRet = checkInstance(inst);
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
	loadModConf->sortFiles = GLOB_NOSORT;
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
	/* new style config has different default! */
#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE) /* use FEN on Solaris! */
	loadModConf->opMode = OPMODE_FEN;
#else
	loadModConf->opMode = OPMODE_INOTIFY;
#endif
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "imfile: error processing module "
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
		} else if(!strcmp(modpblk.descr[i].name, "sortfiles")) {
			loadModConf->sortFiles = ((sbool) pvals[i].val.d.n) ? 0 : GLOB_NOSORT;
		} else if(!strcmp(modpblk.descr[i].name, "mode")) {
			if(!es_strconstcmp(pvals[i].val.d.estr, "polling"))
				loadModConf->opMode = OPMODE_POLLING;
			else if(!es_strconstcmp(pvals[i].val.d.estr, "inotify")) {
#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE) /* use FEN on Solaris! */
				loadModConf->opMode = OPMODE_FEN;
				DBGPRINTF("inotify mode configured, but only FEN "
				"is available on OS SOLARIS. Switching to FEN Mode automatically\n");
#else
				loadModConf->opMode = OPMODE_INOTIFY;
#endif
			} else if(!es_strconstcmp(pvals[i].val.d.estr, "fen"))
				loadModConf->opMode = OPMODE_FEN;
			else {
				char *cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
				LogError(0, RS_RET_PARAM_ERROR, "imfile: unknown "
					"mode '%s'", cstr);
				free(cstr);
			}
		} else {
			DBGPRINTF("program error, non-handled "
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
	DBGPRINTF("opmode is %d, polling interval is %d\n",
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
		LogError(0, RS_RET_NO_LISTNERS,
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
		LogError(0, NO_ERRCODE, "imfile: no file monitors could be started, "
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

//-------------------- NOW NEEDED for FEN as well!
#if defined(HAVE_INOTIFY_INIT) || (defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE))
/* the basedir buffer must be of size MAXFNAME
 * returns the index of the last slash before the basename
 */
static int ATTR_NONNULL()
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

static rsRetVal ATTR_NONNULL()
fileTableInit(fileTable_t *const __restrict__ tab, const int nelem)
{
	DEFiRet;
	CHKmalloc(tab->listeners = malloc(sizeof(dirInfoFiles_t) * nelem));
	tab->allocMax = nelem;
	tab->currMax = 0;
finalize_it:
	RETiRet;
}

#if ULTRA_DEBUG == 1
static void ATTR_NONNULL()
fileTableDisplay(fileTable_t *tab)
{
	int f;
	uchar *baseName;
	DBGPRINTF("fileTableDisplay = dirs.currMaxfiles %d\n", tab->currMax);
	for(f = 0 ; f < tab->currMax ; ++f) {
		baseName = tab->listeners[f].pLstn->pszBaseName;
		DBGPRINTF("fileTableDisplay = TABLE %p CONTENTS, %d->%p:'%s'\n",
			tab, f, tab->listeners[f].pLstn, (char*)baseName);
	}
}
#endif

static int ATTR_NONNULL()
fileTableSearch(fileTable_t *const __restrict__ tab, uchar *const __restrict__ fn)
{
	int f;
	uchar *baseName = NULL;
	#if ULTRA_DEBUG == 1
		fileTableDisplay(tab);
	#endif
	for(f = 0 ; f < tab->currMax ; ++f) {
		baseName = tab->listeners[f].pLstn->pszBaseName;
		if(!fnmatch((char*)baseName, (char*)fn, FNM_PATHNAME | FNM_PERIOD))
			break; /* found */
	}
	if(f == tab->currMax)
		f = -1;
	DBGPRINTF("fileTableSearch file '%s' - '%s', found:%d\n", fn, (f == -1) ? NULL : baseName, f);
	return f;
}

static int ATTR_NONNULL()
fileTableSearchNoWildcard(fileTable_t *const __restrict__ tab, uchar *const __restrict__ fn)
{
	int f;
	uchar *baseName = NULL;
	for(f = 0 ; f < tab->currMax ; ++f) {
		baseName = tab->listeners[f].pLstn->pszBaseName;
		if (strcmp((const char*)baseName, (const char*)fn) == 0)
			break; /* found */
	}
	if(f == tab->currMax)
		f = -1;
	DBGPRINTF("fileTableSearchNoWildcard file '%s' - '%s', found:%d\n", fn, baseName, f);
	return f;
}

/* add file to file table */
static rsRetVal ATTR_NONNULL()
fileTableAddFile(fileTable_t *const __restrict__ tab, lstn_t *const __restrict__ pLstn)
{
	int j;
	DEFiRet;
	for(j = 0 ; j < tab->currMax && tab->listeners[j].pLstn != pLstn ; ++j)
		; /* just scan */
	if(j < tab->currMax) {
		++tab->listeners[j].refcnt;
		DBGPRINTF("file '%s' already registered, refcnt now %d\n",
			pLstn->pszFileName, tab->listeners[j].refcnt);
		FINALIZE;
	}

	if(tab->currMax == tab->allocMax) {
		const int newMax = 2 * tab->allocMax;
		dirInfoFiles_t *newListenerTab = realloc(tab->listeners, newMax * sizeof(dirInfoFiles_t));
		if(newListenerTab == NULL) {
			LogError(0, RS_RET_OUT_OF_MEMORY,
					"cannot alloc memory to map directory/file relationship "
					"for '%s' - ignoring", pLstn->pszFileName);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		tab->listeners = newListenerTab;
		tab->allocMax = newMax;
		DBGPRINTF("increased dir table to %d entries\n", allocMaxDirs);
	}

	tab->listeners[tab->currMax].pLstn = pLstn;
	tab->listeners[tab->currMax].refcnt = 1;
	tab->currMax++;
finalize_it:
	RETiRet;
}

/* delete a file from file table */
static rsRetVal ATTR_NONNULL()
fileTableDelFile(fileTable_t *const __restrict__ tab, lstn_t *const __restrict__ pLstn)
{
	int j;
	DEFiRet;

	for(j = 0 ; j < tab->currMax && tab->listeners[j].pLstn != pLstn ; ++j)
		; /* just scan */
	if(j == tab->currMax) {
		DBGPRINTF("no association for file '%s'\n", pLstn->pszFileName);
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
dirsAdd(const uchar *const dirName, int *const piIndex)
{
	sbool sbAdded;
	int newMax;
	int i, newindex;
	dirInfo_t *newDirTab;
	char* psztmp;
	DEFiRet;

	dbgprintf("dirsAdd: add '%s'\n", dirName);
	/* Set new index to last dir by default, then search for a free spot in dirs array */
	newindex = currMaxDirs;
	sbAdded = TRUE;
	for(i= 0 ; i < currMaxDirs ; ++i) {
		if (dirs[i].dirName == NULL) {
			newindex = i;
			sbAdded = FALSE;
			DBGPRINTF("dirsAdd found free spot at %d, reusing\n", newindex);
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
			LogError(0, RS_RET_OUT_OF_MEMORY,
					"dirsAdd cannot alloc memory to monitor directory '%s' - ignoring",
					dirName);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		dirs = newDirTab;
		allocMaxDirs = newMax;
		DBGPRINTF("dirsAdd increased dir table to %d entries\n", allocMaxDirs);
	}

	/* if we reach this point, there is space in the file table for the new entry */
	CHKmalloc(dirs[newindex].dirName = ustrdup(dirName)); /* Get a copy of the string !*/
	dirs[newindex].dirNameBfWildCard = NULL;
	dirs[newindex].bDirType = DIR_CONFIGURED; /* Default to configured! */
	CHKiRet(fileTableInit(&dirs[newindex].active, INIT_FILE_IN_DIR_TAB_SIZE));
	CHKiRet(fileTableInit(&dirs[newindex].configured, INIT_FILE_IN_DIR_TAB_SIZE));

	/* check for wildcard in directoryname, if last character is a wildcard we remove it and try again! */
	dirs[newindex].hasWildcard = containsGlobWildcard((char*)dirName);
	CHKmalloc(dirs[newindex].dirNameBfWildCard = ustrdup(dirName));

	/* Default rootidx always -1 */
	dirs[newindex].rdiridx = -1;

	/* Extrac checking for wildcard mode */
	if (dirs[newindex].hasWildcard) {
		// TODO: wildcard is not necessarily in last char!!!
		// TODO: BUG: we have many more wildcards that "*" - so this check is invalid
		DBGPRINTF("dirsAdd detected wildcard in dir '%s'\n", dirName);

		/* Set NULL Byte to FIRST wildcard occurrence */
		psztmp = strchr((char*)dirs[newindex].dirNameBfWildCard, '*');
		if (psztmp != NULL) {
			*psztmp = '\0';
			/*	Now set NULL Byte on last directory delimiter occurrence,
			*	This makes sure that we have the current base path to create a watch for! */
			psztmp = strrchr((char*)dirs[newindex].dirNameBfWildCard, '/');
			if (psztmp != NULL) {
				*psztmp = '\0';
			} else {
				LogError(0, RS_RET_SYS_ERR , "imfile: dirsAdd unexpected error #1 "
					"for dir '%s' ", dirName);
				ABORT_FINALIZE(RS_RET_SYS_ERR);
			}
		} else {
			LogError(0, RS_RET_SYS_ERR , "imfile: dirsAdd unexpected error #2 "
				"for dir '%s' ", dirName);
			ABORT_FINALIZE(RS_RET_SYS_ERR);
		}
		DBGPRINTF("dirsAdd after wildcard removal: '%s'\n",
			dirs[newindex].dirNameBfWildCard);
	}

#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
	// Create FileInfo struct
	dirs[newindex].pfinf = malloc(sizeof(struct fileinfo));
	if (dirs[newindex].pfinf == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY, "imfile: dirsAdd alloc memory "
			"for fileinfo failed ");
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	/* Use parent dir on Wildcard pathes */
	if (dirs[newindex].hasWildcard) {
		if ((dirs[newindex].pfinf->fobj.fo_name = strdup((char*)dirs[newindex].dirNameBfWildCard)) == NULL) {
			LogError(0, RS_RET_OUT_OF_MEMORY, "imfile: dirsAdd alloc memory "
				"for strdup failed ");
			free(dirs[newindex].pfinf);
			dirs[newindex].pfinf = NULL;
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}

	} else {
		if ((dirs[newindex].pfinf->fobj.fo_name = strdup((char*)dirs[newindex].dirName)) == NULL) {
			LogError(0, RS_RET_OUT_OF_MEMORY, "imfile: dirsAdd alloc memory "
				"for strdup failed ");
			free(dirs[newindex].pfinf);
			dirs[newindex].pfinf = NULL;
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
	}

	/* Event types to watch. */
	dirs[newindex].pfinf->events = FILE_MODIFIED; // |FILE_DELETE|FILE_RENAME_TO|FILE_RENAME_FROM;
	dirs[newindex].pfinf->port = glport;
	dirs[newindex].bPortAssociated = 0;
#endif

	DBGPRINTF("dirsAdd added dir %d to dirs table: '%s'\n", newindex, dirName);
	/* Increment only if entry was added and not reused */
	if (sbAdded == TRUE)
		++currMaxDirs;
finalize_it:
	RETiRet;
}


/* checks if a dir name is already inside the dirs array. If so, returns
 * its index. If not present, -1 is returned.
 */
static int ATTR_NONNULL(1)
dirsFindDir(const uchar *const dir)
{
	int i;
	int iFind = -1;

	for(i = 0 ; i < currMaxDirs ; ++i) {
		if (dirs[i].dirName != NULL && ustrcmp(dir, dirs[i].dirName) == 0) {
			iFind = i;
			break;
		}
	}
	DBGPRINTF("dirsFindDir: '%s' - idx %d\n", dir, iFind);
	return iFind;
}

static rsRetVal
dirsInit(void)
{
	instanceConf_t *inst;
	DEFiRet;

DBGPRINTF("dirsInit\n");
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
static rsRetVal ATTR_NONNULL(1)
dirsAddFile(lstn_t *__restrict__ pLstn, const int bActive)
{
	int dirIdx;
	dirInfo_t *dir;
	DEFiRet;

	dirIdx = dirsFindDir(pLstn->pszDirName);
	if(dirIdx == -1) {
		LogError(0, RS_RET_INTERNAL_ERROR, "imfile: could not find "
			"directory '%s' in dirs array - ignoring",
			pLstn->pszDirName);
		FINALIZE;
	}

	dir = dirs + dirIdx;
	CHKiRet(fileTableAddFile((bActive ? &dir->active : &dir->configured), pLstn));
	DBGPRINTF("associated file [%s] to directory %d[%s], Active = %d\n",
		pLstn->pszFileName, dirIdx, dir->dirName, bActive);
	#if ULTRA_DEBUG == 1
	/* UNCOMMENT FOR DEBUG fileTableDisplay(bActive ? &dir->active : &dir->configured); */
	#endif
finalize_it:
	RETiRet;
}

/* Duplicate an existing listener. This is called when a new file is to
 * be monitored due to wildcard detection. Returns the new pLstn in
 * the ppExisting parameter.
 */
static rsRetVal ATTR_NONNULL(1, 2)
lstnDup(lstn_t ** ppExisting,
	const uchar *const __restrict__ newname,
	const uchar *const __restrict__ newdirname)
{
	DEFiRet;
	lstn_t *const existing = *ppExisting;
	lstn_t *pThis;
	CHKiRet(lstnAdd(&pThis));

	/* Use dynamic dirname if newdirname is set! */
	if (newdirname == NULL) {
		pThis->pszDirName = existing->pszDirName; /* read-only */
	} else {
		CHKmalloc(pThis->pszDirName = ustrdup(newdirname));
	}
	CHKmalloc(pThis->pszBaseName = ustrdup(newname));
	if(asprintf((char**)&pThis->pszFileName, "%s/%s",
		(char*)pThis->pszDirName, (char*)newname) == -1) {
		DBGPRINTF("lstnDup: asprintf failed, malfunction can happen\n");
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	CHKmalloc(pThis->pszTag = ustrdup(existing->pszTag));
	pThis->lenTag = ustrlen(pThis->pszTag);
	if(existing->pszStateFile == NULL) {
		pThis->pszStateFile = NULL;
	} else {
		CHKmalloc(pThis->pszStateFile = ustrdup(existing->pszStateFile));
	}

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
			DBGPRINTF("error regex compile\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}
	pThis->discardTruncatedMsg = existing->discardTruncatedMsg;
	pThis->msgDiscardingError = existing->msgDiscardingError;
	pThis->bRMStateOnDel = existing->bRMStateOnDel;
	pThis->hasWildcard = existing->hasWildcard;
	pThis->escapeLF = existing->escapeLF;
	pThis->reopenOnTruncate = existing->reopenOnTruncate;
	pThis->addMetadata = existing->addMetadata;
	pThis->addCeeTag = existing->addCeeTag;
	pThis->readTimeout = existing->readTimeout;
	pThis->freshStartTail = existing->freshStartTail;
	pThis->fileNotFoundError = existing->fileNotFoundError;
	pThis->pRuleset = existing->pRuleset;
	pThis->nRecords = 0;
	pThis->pStrm = NULL;
	pThis->prevLineSegment = NULL;
	pThis->masterLstn = existing;
	#ifdef HAVE_INOTIFY_INIT
		pThis->movedfrom_statefile = NULL;
		pThis->movedfrom_cookie = 0;
	#endif
	#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
		pThis->pfinf = NULL;
		pThis->bPortAssociated = 0;
	#endif
	*ppExisting = pThis;
finalize_it:
	RETiRet;
}
#endif /* #if defined(HAVE_INOTIFY_INIT) || (defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)) --- */

#ifdef HAVE_INOTIFY_INIT
static void
in_setupDirWatch(const int dirIdx)
{
	const int wd = inotify_add_watch(ino_fd, (char*)dirs[dirIdx].dirNameBfWildCard,
		IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO);
	if(wd < 0) {
		LogError(errno, RS_RET_IO_ERROR, "imfile: cannot watch directory '%s'",
			dirs[dirIdx].dirNameBfWildCard);
		goto done;
	}
	wdmapAdd(wd, dirIdx, NULL);
	DBGPRINTF("in_setupDirWatch: watch %d added for dir %s(Idx=%d)\n", wd,
		(char*) dirs[dirIdx].dirNameBfWildCard, dirIdx);
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
static void ATTR_NONNULL(1)
startLstnFile(lstn_t *const __restrict__ pLstn)
{
	rsRetVal localRet;
	DBGPRINTF("startLstnFile for file '%s'\n", pLstn->pszFileName);
	const int wd = inotify_add_watch(ino_fd, (char*)pLstn->pszFileName, IN_MODIFY);
	if(wd < 0) {
		if(pLstn->fileNotFoundError) {
			LogError(errno, NO_ERRCODE, "imfile: error with inotify API,"
					" ignoring file '%s'", pLstn->pszFileName);
		} else {
			char errStr[512];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("could not create file table entry for '%s' - "
				  "not processing it now: %s\n", pLstn->pszFileName, errStr);
		}
		goto done;
	}
	if((localRet = wdmapAdd(wd, -1, pLstn)) != RS_RET_OK) {
		if(	localRet != RS_RET_FILE_ALREADY_IN_TABLE &&
			pLstn->fileNotFoundError) {
			LogError(0, NO_ERRCODE, "imfile: internal error: error %d adding file "
					"to wdmap, ignoring file '%s'\n", localRet, pLstn->pszFileName);
		} else {
			DBGPRINTF("error %d adding file to wdmap, ignoring\n", localRet);
		}
		goto done;
	}
	DBGPRINTF("watch %d added for file %s\n", wd, pLstn->pszFileName);
	dirsAddFile(pLstn, ACTIVE_FILE);
	pollFile(pLstn, NULL);
done:	return;
}

/* Setup a new file watch for dynamically discovered files (via wildcards).
 * Note: we need to try to read this file, as it may already contain data this
 * needs to be processed, and we won't get an event for that as notifications
 * happen only for things after the watch has been activated.
 */
static void ATTR_NONNULL(1)
in_setupFileWatchDynamic(lstn_t *pLstn,
	uchar *const __restrict__ newBaseName,
	uchar *const __restrict__ newFileName)
{
	char fullfn[MAXFNAME];
	struct stat fileInfo;
	uchar basedir[MAXFNAME];
	uchar* pBaseDir = NULL;
	int idirindex;

	if (newFileName == NULL) {
		snprintf(fullfn, MAXFNAME, "%s/%s", pLstn->pszDirName, newBaseName);
	} else {
		/* Get BaseDir from filename! */
		getBasedir(basedir, newFileName);
		pBaseDir = &basedir[0]; /* Set BaseDir Pointer */
		idirindex = dirsFindDir(basedir);
		if (idirindex == -1) {
			/* Add dir to table and create watch */
			DBGPRINTF("Adding new dir '%s' to dirs table \n", basedir);
			dirsAdd(basedir, &idirindex);
			in_setupDirWatch(idirindex);
		}
		/* Use newFileName */
		snprintf(fullfn, MAXFNAME, "%s", newFileName);
	}
	if(stat(fullfn, &fileInfo) != 0) {
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		DBGPRINTF("ignoring file '%s' cannot stat(): %s\n",
			fullfn, errStr);
		goto done;
	}

	if(S_ISDIR(fileInfo.st_mode)) {
		DBGPRINTF("ignoring directory '%s'\n", fullfn);
		goto done;
	}

	if(lstnDup(&pLstn, newBaseName, pBaseDir) != RS_RET_OK)
		goto done;

	startLstnFile(pLstn);
done:	return;
}

/* Search for matching files using glob.
 * Create Dynamic Watch for each found file
 */
static void
in_setupFileWatchGlobSearch(lstn_t *pLstn)
{
	int wd;

	DBGPRINTF("in_setupFileWatchGlobSearch file '%s' has wildcard, doing initial expansion\n",
		pLstn->pszFileName);
	glob_t files;
	const int ret = glob((char*)pLstn->pszFileName,
				GLOB_MARK|GLOB_NOSORT|GLOB_BRACE, NULL, &files);
	if(ret == 0) {
		for(unsigned i = 0 ; i < files.gl_pathc ; i++) {
			uchar basen[MAXFNAME];
			uchar *const file = (uchar*)files.gl_pathv[i];

			if(file[strlen((char*)file)-1] == '/')
				continue;/* we cannot process subdirs! */

			/* search for existing watched files !*/
			wd = wdmapLookupFilename(file);
			if(wd >= 0) {
				DBGPRINTF("in_setupFileWatchGlobSearch '%s' already watched in wd %d\n",
					file, wd);
			} else {
				getBasename(basen, file);
				DBGPRINTF("in_setupFileWatchGlobSearch setup dynamic watch "
					"for '%s : %s' \n", basen, file);
				in_setupFileWatchDynamic(pLstn, basen, file);
			}
		}
		globfree(&files);
	}
}

/* Setup a new file watch for static (configured) files.
 * Note: we need to try to read this file, as it may already contain data this
 * needs to be processed, and we won't get an event for that as notifications
 * happen only for things after the watch has been activated.
 */
static void ATTR_NONNULL(1)
in_setupFileWatchStatic(lstn_t *pLstn)
{
	sbool hasWildcard;

	DBGPRINTF("in_setupFileWatchStatic: adding file '%s' to configured table\n",
		  pLstn->pszFileName);
	dirsAddFile(pLstn, CONFIGURED_FILE);

	/* perform wildcard check on static files manually */
	hasWildcard = containsGlobWildcard((char*)pLstn->pszFileName);
	if(hasWildcard) {
		/* search for matching files */
		in_setupFileWatchGlobSearch(pLstn);
	} else {
		/* Duplicate static object as well, otherwise the configobject
		 * could be deleted later! */
		if(lstnDup(&pLstn, pLstn->pszBaseName, NULL) != RS_RET_OK) {
			DBGPRINTF("in_setupFileWatchStatic failed to duplicate listener for '%s'\n",
				pLstn->pszFileName);
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
	DBGPRINTF("setting up initial directory watches\n");
	for(i = 0 ; i < currMaxDirs ; ++i) {
		in_setupDirWatch(i);
	}
	lstn_t *pLstn;
	DBGPRINTF("setting up initial listener watches\n");
	for(pLstn = runModConf->pRootLstn ; pLstn != NULL ; pLstn = pLstn->next) {
		if(pLstn->masterLstn == NULL) {
			/* we process only static (master) entries */
			in_setupFileWatchStatic(pLstn);
		}
	}
}

static void ATTR_NONNULL(1)
in_dbg_showEv(struct inotify_event *ev)
{
	if(!Debug)
		return;
	if(ev->mask & IN_IGNORED) {
		dbgprintf("INOTIFY event: watch was REMOVED\n");
	} else if(ev->mask & IN_MODIFY) {
		dbgprintf("INOTIFY event: watch was MODIFID\n");
	} else if(ev->mask & IN_ACCESS) {
		dbgprintf("INOTIFY event: watch IN_ACCESS\n");
	} else if(ev->mask & IN_ATTRIB) {
		dbgprintf("INOTIFY event: watch IN_ATTRIB\n");
	} else if(ev->mask & IN_CLOSE_WRITE) {
		dbgprintf("INOTIFY event: watch IN_CLOSE_WRITE\n");
	} else if(ev->mask & IN_CLOSE_NOWRITE) {
		dbgprintf("INOTIFY event: watch IN_CLOSE_NOWRITE\n");
	} else if(ev->mask & IN_CREATE) {
		dbgprintf("INOTIFY event: file was CREATED: %s\n", ev->name);
	} else if(ev->mask & IN_DELETE) {
		dbgprintf("INOTIFY event: watch IN_DELETE\n");
	} else if(ev->mask & IN_DELETE_SELF) {
		dbgprintf("INOTIFY event: watch IN_DELETE_SELF\n");
	} else if(ev->mask & IN_MOVE_SELF) {
		dbgprintf("INOTIFY event: watch IN_MOVE_SELF\n");
	} else if(ev->mask & IN_MOVED_FROM) {
		dbgprintf("INOTIFY event: watch IN_MOVED_FROM\n");
	} else if(ev->mask & IN_MOVED_TO) {
		dbgprintf("INOTIFY event: watch IN_MOVED_TO\n");
	} else if(ev->mask & IN_OPEN) {
		dbgprintf("INOTIFY event: watch IN_OPEN\n");
	} else if(ev->mask & IN_ISDIR) {
		dbgprintf("INOTIFY event: watch IN_ISDIR\n");
	} else {
		dbgprintf("INOTIFY event: unknown mask code %8.8x\n", ev->mask);
	 }
}

/* Helper function to get fullpath when handling inotify dir events */
static void ATTR_NONNULL()
in_handleDirGetFullDir(char *const pszoutput, const int dirIdx, const char *const pszsubdir)
{
	assert(dirIdx >= 0);
	DBGPRINTF("in_handleDirGetFullDir root='%s' sub='%s' \n", dirs[dirIdx].dirName, pszsubdir);
	snprintf(pszoutput, MAXFNAME, "%s/%s", dirs[dirIdx].dirNameBfWildCard, pszsubdir);
}

/* inotify told us that a file's wd was closed. We now need to remove
 * the file from our internal structures. Remember that a different inode
 * with the same name may already be in processing.
 */
static void ATTR_NONNULL(2)
in_removeFile(const int dirIdx, lstn_t *const __restrict__ pLstn, const sbool bRemoveStateFile)
{
	uchar statefile[MAXFNAME];
	uchar toDel[MAXFNAME];
	int bDoRMState;
	int wd;
	uchar *statefn;

	DBGPRINTF("remove listener '%s', dirIdx %d\n", pLstn->pszFileName, dirIdx);
	if(bRemoveStateFile == TRUE && pLstn->bRMStateOnDel) {
		statefn = getStateFileName(pLstn, statefile, sizeof(statefile), NULL);
		/* Get full path and file name */
		getFullStateFileName(statefn, toDel, sizeof(toDel));
		bDoRMState = 1;
	} else {
		bDoRMState = 0;
	}
	pollFile(pLstn, NULL); /* one final try to gather data */
	/* delete listener data */
	DBGPRINTF("DELETING listener data for '%s' - '%s'\n", pLstn->pszBaseName, pLstn->pszFileName);
	lstnDel(pLstn);
	fileTableDelFile(&dirs[dirIdx].active, pLstn);
	if(bDoRMState) {
		DBGPRINTF("unlinking '%s'\n", toDel);
		if(unlink((char*)toDel) != 0) {
			LogError(errno, RS_RET_ERR, "imfile: could not remove state "
				"file '%s'", toDel);
		}
	}

	wd = wdmapLookupListner(pLstn);
	wdmapDel(wd);
}

static void ATTR_NONNULL(1)
in_handleDirEventDirCREATE(struct inotify_event *ev, const int dirIdx)
{
	char fulldn[MAXFNAME];
	int newdiridx;
	int iListIdx;
	sbool hasWildcard;

	/* Combine to Full Path first */
	in_handleDirGetFullDir(fulldn, dirIdx, (char*)ev->name);

	/* Search for existing entry first! */
	newdiridx = dirsFindDir( (uchar*)fulldn );
	if(newdiridx == -1) {
		/* Add dir to table and create watch */
		DBGPRINTF("in_handleDirEventDirCREATE Adding new dir '%s' to dirs table \n", fulldn);
		dirsAdd((uchar*)fulldn, &newdiridx);
		dirs[newdiridx].bDirType = DIR_DYNAMIC; /* Set to DYNAMIC directory! */
		in_setupDirWatch(newdiridx);
		/* Set propper root index for newdiridx */
		dirs[newdiridx].rdiridx = (dirs[dirIdx].rdiridx != -1 ? dirs[dirIdx].rdiridx : dirIdx);

		DBGPRINTF("in_handleDirEventDirCREATE wdentry dirIdx=%d, rdirIdx=%d, dirIdxName=%s, dir=%s)\n",
			dirIdx, dirs[newdiridx].rdiridx, dirs[dirIdx].dirName, fulldn);
		if (dirs[dirs[newdiridx].rdiridx].configured.currMax > 0) {
			DBGPRINTF("in_handleDirEventDirCREATE found configured listeners\n");

			/* Loop through configured Listeners and scan for dynamic files */
			for(iListIdx = 0; iListIdx < dirs[dirs[newdiridx].rdiridx].configured.currMax; iListIdx++) {
				hasWildcard = (	dirs[dirs[newdiridx].rdiridx].hasWildcard ||
					dirs[dirs[newdiridx].rdiridx].configured.listeners[iListIdx].pLstn->hasWildcard
						? TRUE : FALSE);
				if (hasWildcard == 1){
					DBGPRINTF("in_handleDirEventDirCREATE configured listener has Wildcard\n");
					/* search for matching files */
					in_setupFileWatchGlobSearch(
						dirs[dirs[newdiridx].rdiridx].configured.listeners[iListIdx].pLstn);
				}
			}
		}
	} else {
		DBGPRINTF("dir '%s' already exists in dirs table (Idx %d)\n", fulldn, newdiridx);
	}
}

static void ATTR_NONNULL(1)
in_handleDirEventFileCREATE(struct inotify_event *const ev, const int dirIdx)
{
	int i;
	lstn_t *pLstn = NULL;
	int ftIdx;
	char fullfn[MAXFNAME];
	uchar statefile_new[MAXFNAME];
	uchar statefilefull_old[MAXFNAME];
	uchar statefilefull_new[MAXFNAME];
	uchar* pszDir = NULL;
	int dirIdxFinal = dirIdx;
	ftIdx = fileTableSearch(&dirs[dirIdxFinal].active, (uchar*)ev->name);
	if(ftIdx >= 0) {
		pLstn = dirs[dirIdxFinal].active.listeners[ftIdx].pLstn;
	} else {
		DBGPRINTF("in_handleDirEventFileCREATE  '%s' not associated with dir '%s' "
			"(CurMax:%d, DirIdx:%d, DirType:%s)\n", ev->name, dirs[dirIdxFinal].dirName,
			dirs[dirIdxFinal].active.currMax, dirIdxFinal,
			(dirs[dirIdxFinal].bDirType == DIR_CONFIGURED ? "configured" : "dynamic") );
		ftIdx = fileTableSearch(&dirs[dirIdxFinal].configured, (uchar*)ev->name);
		if(ftIdx == -1) {
			if (dirs[dirIdxFinal].bDirType == DIR_DYNAMIC) {
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
							dirIdxFinal = i; /* Have to correct directory index for
							listnr dupl in in_setupFileWatchDynamic */

							DBGPRINTF("Found matching directory for file '%s' in "
								"dir '%s' (Idx=%d)\n", ev->name,
								dirs[dirIdxFinal].dirName, dirIdxFinal);
							break;
						}
					}
					/* Found Listener to se */
					pLstn = dirs[dirIdxFinal].configured.listeners[ftIdx].pLstn;

					if(ftIdx == -1) {
						DBGPRINTF("file '%s' not associated with dir '%s' and also no "
							"matching wildcard directory found\n", ev->name,
							dirs[dirIdxFinal].dirName);
						goto done;
					}
					else {
						DBGPRINTF("file '%s' not associated with dir '%s', using "
							"dirIndex %d instead\n", ev->name, (pszDir == NULL)
							? dirs[dirIdxFinal].dirName : pszDir, dirIdxFinal);
					}
				} else {
					DBGPRINTF("file '%s' not associated with dir '%s'\n",
						ev->name, dirs[dirIdxFinal].dirName);
					goto done;
				}
			}
		} else
			pLstn = dirs[dirIdxFinal].configured.listeners[ftIdx].pLstn;
	}
	if (pLstn != NULL)	{
		DBGPRINTF("file '%s' associated with dir '%s' (Idx=%d)\n",
			ev->name, (pszDir == NULL) ? dirs[dirIdxFinal].dirName : pszDir, dirIdxFinal);

		/* We need to check if we have a preexisting statefile and move it*/
		if(ev->mask & IN_MOVED_TO) {
			if (pLstn->movedfrom_statefile != NULL && pLstn->movedfrom_cookie == ev->cookie) {
				/* We need to prepar fullfn before we can generate statefilename */
				snprintf(fullfn, MAXFNAME, "%s/%s", (pszDir == NULL) ? dirs[dirIdxFinal].dirName
					: pszDir, (uchar*)ev->name);
				getStateFileName(NULL, statefile_new, sizeof(statefile_new), (uchar*)fullfn);
				getFullStateFileName(statefile_new, statefilefull_new, sizeof(statefilefull_new));
				getFullStateFileName(pLstn->movedfrom_statefile, statefilefull_old,
					sizeof(statefilefull_old));

				DBGPRINTF("old statefile '%s' needs to be moved to '%s' first!\n",
				statefilefull_old, statefilefull_new);

//				openStateFileAndMigrate(pLstn, &statefilefull_old[0], &statefilefull_new[0]);
				if(rename((char*) &statefilefull_old, (char*) &statefilefull_new) != 0) {
					LogError(errno, RS_RET_ERR, "imfile: could not rename statefile "
						"'%s' into '%s'", statefilefull_old, statefilefull_new);
				} else {
					DBGPRINTF("statefile '%s' renamed into '%s'\n", statefilefull_old,
						statefilefull_new);
				}

				/* Free statefile memory */
				free(pLstn->movedfrom_statefile);
				pLstn->movedfrom_statefile = NULL;
				pLstn->movedfrom_cookie = 0;
			} else {
					DBGPRINTF("IN_MOVED_TO either unknown cookie '%d' we expected '%d' or "
						"missing statefile '%s'\n", pLstn->movedfrom_cookie,
						ev->cookie, pLstn->movedfrom_statefile);
			}
		}

		/* Setup a watch now for new file*/
		in_setupFileWatchDynamic(pLstn, (uchar*)ev->name, (pszDir == NULL) ? NULL : (uchar*)fullfn);
	}
done:	return;
}

/* note: we need to care only for active files in the DELETE case.
 * Two reasons: a) if this is a configured file, it should be active
 * b) if not for some reason, there still is nothing we can do against
 * it, and trying to process a *deleted* file really makes no sense
 * (remeber we don't have it open, so it actually *is gone*).
 */
static void ATTR_NONNULL(1)
in_handleDirEventFileDELETE(struct inotify_event *const ev, const int dirIdx)
{
	const int ftIdx = fileTableSearch(&dirs[dirIdx].active, (uchar*)ev->name);
	if(ftIdx == -1) {
		DBGPRINTF("deleted file '%s' not active in dir '%s'\n",
			ev->name, dirs[dirIdx].dirName);
		goto done;
	}
	DBGPRINTF("imfile delete processing for '%s'\n",
		dirs[dirIdx].active.listeners[ftIdx].pLstn->pszFileName);
	in_removeFile(dirIdx, dirs[dirIdx].active.listeners[ftIdx].pLstn, TRUE);
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
	DBGPRINTF("in_removeDir remove '%s', dirIdx=%d wdindex=%d\n", dirs[dirIdx].dirName, dirIdx, wd);
	wdmapDel(wd);
}

static void ATTR_NONNULL(1)
in_handleDirEventDirDELETE(struct inotify_event *const ev, const int dirIdx)
{
	char fulldn[MAXFNAME];
	int finddiridx;

	in_handleDirGetFullDir(fulldn, dirIdx, (char*)ev->name);

	/* Search for existing entry first! */
	finddiridx = dirsFindDir( (uchar*)fulldn );

	if(finddiridx != -1) {
		/* Remove internal structures */
		in_removeDir(finddiridx);

		/* Delete dir from dirs array! */
		free(dirs[finddiridx].dirName);
		if (dirs[finddiridx].dirNameBfWildCard != NULL)
			free(dirs[finddiridx].dirNameBfWildCard);
		free(dirs[finddiridx].active.listeners);
		free(dirs[finddiridx].configured.listeners);
		dirs[finddiridx].dirName = NULL;
		dirs[finddiridx].dirNameBfWildCard = NULL;

		DBGPRINTF("in_handleDirEventDirDELETE dir (idx %d) '%s' deleted \n", finddiridx, fulldn);
	} else {
		DBGPRINTF("in_handleDirEventDirDELETE ERROR could not found '%s' in dirs table!\n", fulldn);
	}
}

static void ATTR_NONNULL(1)
in_handleDirEvent(struct inotify_event *const ev, const int dirIdx)
{
	DBGPRINTF("in_handleDirEvent dir event for (Idx %d)%s (mask %x)\n",
		dirIdx, dirs[dirIdx].dirName, ev->mask);
	if((ev->mask & IN_CREATE)) {
		/* TODO: does IN_MOVED_TO make sense here ? */
		if((ev->mask & IN_ISDIR) || (ev->mask & IN_MOVED_TO)) {
			in_handleDirEventDirCREATE(ev, dirIdx); /* Create new Dir */
		} else {
			in_handleDirEventFileCREATE(ev, dirIdx); /* Create new File */
		}
	} else if((ev->mask & IN_DELETE)) {
		if((ev->mask & IN_ISDIR)) {
			in_handleDirEventDirDELETE(ev, dirIdx);	/* Create new Dir */
		} else {
			in_handleDirEventFileDELETE(ev, dirIdx);/* Delete File from dir filetable */
		}
	} else if((ev->mask & IN_MOVED_TO)) {
		if((ev->mask & IN_ISDIR)) {
			in_handleDirEventDirCREATE(ev, dirIdx); /* Create new Dir */
		} else {
			in_handleDirEventFileCREATE(ev, dirIdx); /* Create new File */
		}
	} else {
		DBGPRINTF("got non-expected inotify event:\n");
		in_dbg_showEv(ev);
	}
}


static void ATTR_NONNULL(1, 2)
in_handleFileEvent(struct inotify_event *ev, const wd_map_t *const etry)
{
	if(ev->mask & IN_MODIFY) {
		pollFile(etry->pLstn, NULL);
	} else {
		DBGPRINTF("got non-expected inotify event:\n");
		in_dbg_showEv(ev);
	}
}

static void ATTR_NONNULL(1)
in_processEvent(struct inotify_event *ev)
{
	wd_map_t *etry;
	lstn_t *pLstn;
	int iRet;
	int ftIdx;
	int wd;
	uchar statefile[MAXFNAME];

	if(ev->mask & IN_IGNORED) {
		goto done;
	} else if(ev->mask & IN_MOVED_FROM) {
		/* Find wd entry and remove it */
		etry =  wdmapLookup(ev->wd);
		if(etry != NULL) {
			ftIdx = fileTableSearchNoWildcard(&dirs[etry->dirIdx].active, (uchar*)ev->name);
			DBGPRINTF("IN_MOVED_FROM Event (ftIdx=%d, name=%s)\n", ftIdx, ev->name);
			if(ftIdx >= 0) {
				/* Find listener and wd table index*/
				pLstn = dirs[etry->dirIdx].active.listeners[ftIdx].pLstn;
				wd = wdmapLookupListner(pLstn);

				/* Remove file from inotify watch */
				iRet = inotify_rm_watch(ino_fd, wd); /* Note this will TRIGGER IN_IGNORED Event! */
				if (iRet != 0) {
					DBGPRINTF("inotify_rm_watch error %d (ftIdx=%d, wd=%d, name=%s)\n",
						errno, ftIdx, wd, ev->name);
				} else {
					DBGPRINTF("inotify_rm_watch successfully removed file from watch "
						"(ftIdx=%d, wd=%d, name=%s)\n", ftIdx, wd, ev->name);
				}

				/* Store statefile name for later MOVED_TO event along with COOKIE */
				pLstn->masterLstn->movedfrom_statefile = ustrdup(getStateFileName(pLstn,
					statefile, sizeof(statefile), NULL) );
				pLstn->masterLstn->movedfrom_cookie = ev->cookie;

				/* do NOT remove statefile in this case!*/
				in_removeFile(etry->dirIdx, pLstn, FALSE);
				DBGPRINTF("IN_MOVED_FROM Event file removed file (wd=%d, name=%s)\n", wd, ev->name);
			}
		}
		goto done;
	}
	DBGPRINTF("in_processEvent process Event %x for %s\n", ev->mask, (uchar*)ev->name);

	etry =  wdmapLookup(ev->wd);
	if(etry == NULL) {
		DBGPRINTF("could not lookup wd %d\n", ev->wd);
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
	DBGPRINTF("readTimeouts are configured, checking if some apply\n");

	for(i = 0 ; i < nWdmap ; ++i) {
		DBGPRINTF("wdmap %d, plstn %p\n", i, wdmap[i].pLstn);
		lstn_t *const pLstn = wdmap[i].pLstn;
		if(pLstn != NULL && strmReadMultiLine_isTimedOut(pLstn->pStrm)) {
			DBGPRINTF("wdmap %d, timeout occured\n", i);
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

dbgprintf("pre wdmapinit\n");
	CHKiRet(wdmapInit());
dbgprintf("pre dirsinit\n");
	CHKiRet(dirsInit());
dbgprintf("pre inotify_init\n");
	ino_fd = inotify_init();
	if(ino_fd < 0) {
		LogError(errno, RS_RET_INOTIFY_INIT_FAILED, "imfile: Init inotify "
			"instance failed ");
		return RS_RET_INOTIFY_INIT_FAILED;
	}
	DBGPRINTF("inotify fd %d\n", ino_fd);
dbgprintf("pre setuInitialWatches\n");
	in_setupInitialWatches();
dbgprintf("post setuInitialWatches\n");

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
				LogError(errno, RS_RET_INTERNAL_ERROR,
					"%s:%d: unexpected error during poll timeout wait",
					__FILE__, __LINE__);
				/* we do not abort, as this would render the whole input defunct */
				continue;
			} else if(r != 1) {
				LogError(errno, RS_RET_INTERNAL_ERROR,
					"%s:%d: ERROR: poll returned more fds (%d) than given to it (1)",
					__FILE__, __LINE__, r);
				/* we do not abort, as this would render the whole input defunct */
				continue;
			}
		}
		rd = read(ino_fd, iobuf, sizeof(iobuf));
		if(rd == -1 && errno == EINTR) {
			/* This might have been our termination signal! */
			DBGPRINTF("EINTR received during inotify, restarting poll\n");
			continue;
		}
		if(rd < 0) {
			LogError(errno, RS_RET_IO_ERROR, "imfile: error during inotify - ignored");
			continue;
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
	LogError(0, RS_RET_NOT_IMPLEMENTED, "imfile: mode set to inotify, but the "
			"platform does not support inotify");
	return RS_RET_NOT_IMPLEMENTED;
}
#endif /* #if HAVE_INOTIFY_INIT */


/* --- Monitor files in FEN mode (OS_SOLARIS)*/
#if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE) /* use FEN on Solaris! */
static void
fen_printevent(int event)
{
	if (event & FILE_ACCESS) {
		DBGPRINTF(" FILE_ACCESS");
	}
	if (event & FILE_MODIFIED) {
		DBGPRINTF(" FILE_MODIFIED");
	}
	if (event & FILE_ATTRIB) {
		DBGPRINTF(" FILE_ATTRIB");
	}
	if (event & FILE_DELETE) {
		DBGPRINTF(" FILE_DELETE");
	}
	if (event & FILE_RENAME_TO) {
		DBGPRINTF(" FILE_RENAME_TO");
	}
	if (event & FILE_RENAME_FROM) {
		DBGPRINTF(" FILE_RENAME_FROM");
	}
	if (event & UNMOUNTED) {
		DBGPRINTF(" UNMOUNTED");
	}
	if (event & MOUNTEDOVER) {
		DBGPRINTF(" MOUNTEDOVER");
	}
}

static rsRetVal ATTR_NONNULL(1)
fen_removeFile(lstn_t *pLstn)
{
	struct stat statFile;
	uchar statefile[MAXFNAME];
	uchar toDel[MAXFNAME];
	int bDoRMState;
	uchar *statefn;
	int dirIdx;
	int ftIdx;
	DEFiRet;

	/* Get correct dirindex from basedir! */
	dirIdx = dirsFindDir(pLstn->pszDirName);
	if (dirIdx == -1) {
		/* Add error processing as required, file may have been deleted/moved. */
		LogError(errno, RS_RET_SYS_ERR, "fen_removeFile: Failed to remove file, "
			"unknown directory index for dir '%s'\n", pLstn->pszDirName);
		ABORT_FINALIZE(RS_RET_SYS_ERR);
	}

	if(pLstn->bRMStateOnDel) {
		statefn = getStateFileName(pLstn, statefile, sizeof(statefile), NULL);
		/* Get full path and file name */
		getFullStateFileName(statefn, toDel, sizeof(toDel));
		bDoRMState = 1;
	} else {
		bDoRMState = 0;
	}

	/* one final try to gather data */
	pollFile(pLstn, NULL);

	/* Check for active / configure file */
	if ( (ftIdx = fileTableSearchNoWildcard(&dirs[dirIdx].active, pLstn->pszBaseName)) >= 0) {
		DBGPRINTF("fen_removeFile removing active listener for '%s', dirIdx %d, ftIdx %d\n",
			pLstn->pszFileName, dirIdx, ftIdx);
		fileTableDelFile(&dirs[dirIdx].active, pLstn);	/* Remove from filetable */
		lstnDel(pLstn);					/* Delete Listener now */
	} else {
		DBGPRINTF("fen_removeFile NOT removing configured listener for '%s', dirIdx %d\n",
			pLstn->pszFileName, dirIdx, ftIdx);
	}

	if(bDoRMState) {
		DBGPRINTF("fen_removeFile unlinking '%s'\n", toDel);
		if((stat((const char*) toDel, &statFile) == 0) && unlink((char*)toDel) != 0) {
			LogError(errno, RS_RET_ERR, "fen_removeFile: could not remove state "
				"file \"%s\": %s", toDel);
		}
	}
finalize_it:
	RETiRet;
}

static rsRetVal
fen_removeDir(int dirIdx)
{
	DEFiRet;

	DBGPRINTF("fen_removeDir removing dir (idx %d) '%s' \n", dirIdx, dirs[dirIdx].dirName);

	/* Delete dir from dirs array! */
	free(dirs[dirIdx].dirName);
	free(dirs[dirIdx].dirNameBfWildCard);
	free(dirs[dirIdx].active.listeners);
	free(dirs[dirIdx].configured.listeners);
	dirs[dirIdx].dirName = NULL;
	dirs[dirIdx].dirNameBfWildCard = NULL;
finalize_it:
	RETiRet;
}

static rsRetVal
fen_processEventFile(struct file_obj* fobjp, lstn_t *pLstn, int revents, int dirIdx)
{
	struct stat statFile;
	int ftIdx;
	DEFiRet;

	// Use FileObj from listener if NULL
	if (fobjp== NULL)
		fobjp = &pLstn->pfinf->fobj;

	/* uncomment if needed DBGPRINTF("fen_processEventFile: %s (0x%" PRIXPTR ") ", fobjp->fo_name, pLstn); **/
	DBGPRINTF("fen_processEventFile: %s ", fobjp->fo_name);
	if (revents) {
		fen_printevent(revents);
	}
	DBGPRINTF("\n");

	if (pLstn == NULL) {
		/* Should not be NULL but it case it is abort */
		DBGPRINTF("fen_processEventFile: Listener '%s' for EVENT already deleted, aborting function.\n",
			fobjp->fo_name);
		FINALIZE;
	}

	/* Port needs to be reassociated */
	pLstn->bPortAssociated = 0;

	/* Compare filename first */
	if (strcmp(fobjp->fo_name, (const char*)pLstn->pszFileName) == 0){
		DBGPRINTF("fen_processEventFile: matching file found: '%s'\n", fobjp->fo_name);

		/* Get File Stats */
		if (!(revents & FILE_EXCEPTION) && stat(fobjp->fo_name, &statFile) == -1) {
			const int errno_save = errno;
			DBGPRINTF("fen_processEventFile: Failed to stat file: %s - errno %d\n",
				fobjp->fo_name, errno);
			LogError(errno_save, RS_RET_FILE_NO_STAT, "imfile: file '%s' not found when "
				"receiving notification event", fobjp->fo_name);
			ABORT_FINALIZE(RS_RET_FILE_NO_STAT);
		}

		/*
		* Add what ever processing that needs to be done
		* here. Process received events.
		*/
		if (revents) {
			if (revents & FILE_MODIFIED) {
				/* File has been modified, trigger a pollFile */
				pollFile(pLstn, NULL);
			} else if (revents & FILE_RENAME_FROM) {
				/* File has been renamed which means it was deleted. remove the file*/
				fen_removeFile(pLstn);
				FINALIZE;
			}

			/* check for file exception. Only happens when really bad things happened like
			harddisk removal. We need to re-register he port. */
			if (revents & FILE_EXCEPTION) {
				fen_removeFile(pLstn);
				ABORT_FINALIZE(RS_RET_SYS_ERR);
			}
		}
	} else {
		DBGPRINTF("fen_processEventFile: file '%s' did not match Listener Filename '%s'\n",
			fobjp->fo_name, pLstn->pszFileName);
		ABORT_FINALIZE(RS_RET_SYS_ERR);
	}

	/* Register file event */
	fobjp->fo_atime = statFile.st_atim;
	fobjp->fo_mtime = statFile.st_mtim;
	fobjp->fo_ctime = statFile.st_ctim;
	if (port_associate(glport, PORT_SOURCE_FILE, (uintptr_t)fobjp,
				pLstn->pfinf->events, (void *)pLstn) == -1) {
		/* Add error processing as required, file may have been deleted/moved. */
		LogError(errno, RS_RET_SYS_ERR, "fen_processEventFile: Failed to associated port for file "
			": %s - errno %d\n", fobjp->fo_name, errno);
		ABORT_FINALIZE(RS_RET_SYS_ERR);
	} else {
		/* Port successfull listening now*/
		DBGPRINTF("fen_processEventFile: associated port for file %s\n", fobjp->fo_name);
		pLstn->bPortAssociated = 1;
	}
finalize_it:
	RETiRet;
}

/* Helper function to find matching files for listener */
rsRetVal
fen_DirSearchFiles(lstn_t *pLstn, int dirIdx)
{
	struct file_obj *fobjp = NULL;	/* Helper object */
	rsRetVal localRet;
	int ftIdx;
	int result;
	glob_t files;
	lstn_t *pLstnNew;
	/* Helper chars */
	uchar basedir[MAXFNAME];
	uchar basefilename[MAXFNAME];
	uchar *file;
	DEFiRet;

	DBGPRINTF("fen_DirSearchFiles search for dynamic files with pattern '%s' \n", pLstn->pszFileName);
	result = glob(	(char*)pLstn->pszFileName,
			GLOB_MARK|runModConf->sortFiles|GLOB_BRACE,
			NULL, &files);
	if(result == 0) {
		DBGPRINTF("fen_DirSearchFiles found %d matches with pattern '%s' \n", files.gl_pathc,
			pLstn->pszFileName);
		for(unsigned i = 0 ; i < files.gl_pathc ; i++) {
			/* Get File, Dir and Basename */
			file = (uchar*)files.gl_pathv[i];
			getBasename(basefilename, file);
			getBasedir(basedir, file);

			if(file[strlen((char*)file)-1] == '/')
				continue;/* we cannot process subdirs! */

			DBGPRINTF("fen_DirSearchFiles found matching file '%s' \n", file);

			/* Get correct dirindex from basedir! */
			dirIdx = dirsFindDir(basedir);
			if (dirIdx == -1) {
				/* Add dir to table and create watch */
				CHKiRet(dirsAdd(basedir, &dirIdx));
				DBGPRINTF("fen_DirSearchFiles adding new dir '%s' to dirs table idx %d\n",
					basedir, dirIdx);
//				fen_processEventDir(NULL, dirIdx, 0); /* Monitor child directory as well */
			}

			/* Search for file index here */
			ftIdx = fileTableSearchNoWildcard(&dirs[dirIdx].active, (uchar*)basefilename);
			if(ftIdx >= 0) {
				DBGPRINTF("fen_DirSearchFiles file '%s' idx %d already being monitored ... \n",
					file, ftIdx);
			} else {
				DBGPRINTF("fen_DirSearchFiles setup new monitor for dynamic file '%s' \n", file);

				/* duplicate listener firs */
				pLstnNew = pLstn;
				localRet = lstnDup(&pLstnNew, basefilename, dirs[dirIdx].dirName);
				if(localRet != RS_RET_OK) {
					DBGPRINTF("fen_DirSearchFiles failed to duplicate listener for '%s' "
						"with iRet %d\n", localRet, pLstn->pszFileName);
					ABORT_FINALIZE(localRet);
				}

				// Create FileInfo struct
				pLstnNew->pfinf = malloc(sizeof(struct fileinfo));
				if (pLstnNew->pfinf == NULL) {
					LogError(errno, RS_RET_IO_ERROR, "fen_DirSearchFiles alloc memory "
						"for fileinfo failed ");
					ABORT_FINALIZE(RS_RET_IO_ERROR);
				}
				if ((pLstnNew->pfinf->fobj.fo_name = strdup((char*)pLstnNew->pszFileName)) == NULL) {
					LogError(errno, RS_RET_IO_ERROR, "fen_DirSearchFiles alloc memory "
						"for strdup failed ");
					free(pLstnNew->pfinf);
					pLstnNew->pfinf = NULL;
					ABORT_FINALIZE(RS_RET_IO_ERROR);
				}
				/* Event types to watch. */
				pLstnNew->pfinf->events = FILE_MODIFIED;
				pLstnNew->pfinf->port = glport;

				/* Add Listener to configured dirs tab */
				dirsAddFile(pLstnNew, ACTIVE_FILE);

				DBGPRINTF("fen_DirSearchFiles duplicated listener for '%s/%s' \n",
					pLstnNew->pszDirName, pLstnNew->pszBaseName);

				/* Trigger file processing */
				fen_processEventFile(NULL, pLstnNew, FILE_MODIFIED, dirIdx);
			}

		}
		globfree(&files);
	} else {
		DBGPRINTF("fen_DirSearchFiles found ZERO matches with pattern '%s' \n", pLstn->pszFileName);
	}

finalize_it:
	RETiRet;
}

/* function not used yet, will be needed for wildcards later */
rsRetVal
fen_processEventDir(struct file_obj* fobjp, int dirIdx, int revents)
{
	int iListIdx;
	struct stat statFile;
	sbool hasWildcard;
	DEFiRet;

	DBGPRINTF("fen_processEventDir '%s' (Configured %d)", dirs[dirIdx].dirName,
		dirs[dirIdx].configured.currMax);

	// Use FileObj from dirinfo if NULL
	if (fobjp== NULL)
		fobjp = &dirs[dirIdx].pfinf->fobj;

	/* Port needs to be reassociated */
	dirs[dirIdx].bPortAssociated = 0;

	if (revents) {
		fen_printevent(revents);
		DBGPRINTF("\n");
		DBGPRINTF("fen_processEventDir DIR EVENTS needs to be processed for '%s'('%s')\n",
			fobjp->fo_name, dirs[dirIdx].dirName);

		/* a file was modified */
		if (revents & FILE_MODIFIED) {
			/* LOOP through configured Listeners */
			for(iListIdx = 0; iListIdx < dirs[dirIdx].configured.currMax; iListIdx++) {
				hasWildcard = (	dirs[dirIdx].hasWildcard ||
						dirs[dirIdx].configured.listeners[iListIdx].pLstn->hasWildcard
							? TRUE : FALSE);
				if (hasWildcard == 1){
					/* Handle Wildcard files */
					fen_DirSearchFiles(	dirs[dirIdx].configured.listeners[iListIdx].pLstn,
								dirIdx);
				} else {
					/* Handle fixed configured files */
					if (dirs[dirIdx].configured.listeners[iListIdx].pLstn->bPortAssociated == 0) {
						DBGPRINTF("fen_processEventDir Listener for %s needs to be checked\n",
						dirs[dirIdx].configured.listeners[iListIdx].pLstn->pszFileName);
						/* Need to check if listener file was created! */
						fen_processEventFile(NULL,
							dirs[dirIdx].configured.listeners[iListIdx].pLstn,
							FILE_MODIFIED
						/*dirs[dirIdx].configured.listeners[iListIdx].pLstn->pfinf->events*/,
							dirIdx);
					} else {
						DBGPRINTF("fen_processEventDir Listener for %s already associated\n",
						dirs[dirIdx].configured.listeners[iListIdx].pLstn->pszFileName);
					}
				}
			}
		}
	} else {
		DBGPRINTF("\n");
	}

	/* Check if dir exists */
	if (stat(fobjp->fo_name, &statFile) == 0 && S_ISDIR(statFile.st_mode)) {
		DBGPRINTF("fen_processEventDir '%s'('%s') is a valid directory, associate port\n"
			, fobjp->fo_name, dirs[dirIdx].dirName, errno);
	} else {
		DBGPRINTF("fen_processEventDir Failed to stat directory: '%s'('%s') - errno %d, removing\n"
			, fobjp->fo_name, dirs[dirIdx].dirName, errno);
		fen_removeDir(dirIdx); /* Remove dir */
		ABORT_FINALIZE(RS_RET_FILE_NO_STAT);
	}

	/* Register file event */
	fobjp->fo_atime = statFile.st_atim;
	fobjp->fo_mtime = statFile.st_mtim;
	fobjp->fo_ctime = statFile.st_ctim;
	if (port_associate(glport, PORT_SOURCE_FILE, (uintptr_t)fobjp, dirs[dirIdx].pfinf->events,
		(void *)dirIdx) == -1) {
		/* Add error processing as required, file may have been deleted/moved. */
		LogError(errno, RS_RET_SYS_ERR, "fen_processEventDir: Failed to associated port "
			"for directory: '%s'('%s') - errno %d\n", fobjp->fo_name, dirs[dirIdx].dirName, errno);
		ABORT_FINALIZE(RS_RET_SYS_ERR);
	} else {
		/* Port successfull listening now*/
		DBGPRINTF("fen_processEventDir associated port for dir '%s'('%s')\n"
			, fobjp->fo_name, dirs[dirIdx].dirName);
		dirs[dirIdx].bPortAssociated = 1;
	}
finalize_it:
	RETiRet;

}

/* Setup directory watches, based on user config */
static void
fen_setupDirWatches(void)
{
	int i;
	for(i = 0 ; i < currMaxDirs ; ++i) {
		if (	dirs[i].bPortAssociated == 0 &&
			dirs[i].dirName != NULL		/* Don't check deleted dirs */ ){
			fen_processEventDir(NULL, i, 0);/* no events on init*/
		}
	}
}

/* Setup static file watches, based on user config */
static void
fen_setupFileWatches(void)
{
	/* Listener helper*/
	lstn_t *pLstn;
	int dirIdx;

	/* Additional check for not associated files */
	for(pLstn = runModConf->pRootLstn ; pLstn != NULL ; pLstn = pLstn->next) {
		/* Search for dirIdx in case directory has wildcard */
		dirIdx = dirsFindDir(pLstn->pszDirName);

		if (	pLstn->bPortAssociated == 0 &&
			pLstn->hasWildcard == 0 /* WildCard File Watches are Setup in EventDir */ &&
			(dirIdx >= 0 && dirs[dirIdx].hasWildcard == FALSE)
			){
			/* Check if file exists now */
			fen_processEventFile(NULL, pLstn, pLstn->pfinf->events, dirIdx);
		}
	}
}

static rsRetVal
do_fen(void)
{
	int bHadFileData; /* were there at least one file with data during this run? */
	port_event_t portEvent;
	struct timespec timeout;
	lstn_t *pLstn;			/* Listener helper*/
	struct file_obj *fobjp = NULL;	/* Helper object */
	struct stat statFile;
	DEFiRet;
	rsRetVal iRetTmp = RS_RET_OK;

	/* Set port timeout to 1 second. We need to checkfor unmonitored files during meantime */
	timeout.tv_sec = 1;
	timeout.tv_nsec = 0;

	/* create port instance */
	if ((glport = port_create()) == -1) {
		LogError(errno, RS_RET_FEN_INIT_FAILED, "do_fen INIT Port failed ");
		return RS_RET_FEN_INIT_FAILED;
	}

	/* create port instance */
	CHKiRet(dirsInit());

	/* Loop through all configured listeners */
	for(pLstn = runModConf->pRootLstn ; pLstn != NULL ; pLstn = pLstn->next) {
		if(pLstn->masterLstn == NULL) {
			DBGPRINTF("do_fen process '%s' in '%s'\n", pLstn->pszBaseName, pLstn->pszDirName);
			// Create FileInfo struct
			pLstn->pfinf = malloc(sizeof(struct fileinfo));
			if (pLstn->pfinf == NULL) {
				LogError(errno, RS_RET_FEN_INIT_FAILED, "do_fen: alloc memory "
					"for fileinfo failed ");
				ABORT_FINALIZE(RS_RET_FEN_INIT_FAILED);
			}
			if ((pLstn->pfinf->fobj.fo_name = strdup((char*)pLstn->pszFileName)) == NULL) {
				LogError(errno, RS_RET_FEN_INIT_FAILED, "do_fen: alloc memory "
					"for strdup failed ");
				free(pLstn->pfinf);
				pLstn->pfinf = NULL;
				ABORT_FINALIZE(RS_RET_FEN_INIT_FAILED);
			}

			/* Event types to watch. */
			pLstn->pfinf->events = FILE_MODIFIED;
			/* Not needed/working |FILE_DELETE|FILE_RENAME_TO|FILE_RENAME_FROM;*/
			pLstn->pfinf->port = glport;
		}

		/* Add Listener to configured dirs tab */
		dirsAddFile(pLstn, CONFIGURED_FILE);
	}

	/* Init File watches ONCE */
	fen_setupFileWatches();

	DBGPRINTF("do_fen ENTER monitoring loop \n");
	while(glbl.GetGlobalInputTermState() == 0) {
		DBGPRINTF("do_fen loop begin... \n");

		/* Check for not associated directories and add dir watches */
		fen_setupDirWatches();

		/* Loop through events, if there are any */
		while (!port_get(glport, &portEvent, &timeout)) {
			switch (portEvent.portev_source) {
				case PORT_SOURCE_FILE:
					/* check if file obj is DIR or FILE */
					fobjp = (struct file_obj*) portEvent.portev_object;
					DBGPRINTF("do_fen event received for '%s', processing ... \n",
						fobjp->fo_name);

					/* Check if we habe a DIR or FILE */
					if (stat(fobjp->fo_name, &statFile) == 0 && S_ISDIR(statFile.st_mode)) {
						fen_processEventDir(fobjp, (int)portEvent.portev_user,
							portEvent.portev_events);
					} else {
						/* Call file events event handler */
						fen_processEventFile(fobjp, (lstn_t*)portEvent.portev_user,
							portEvent.portev_events, -1 /* Unknown diridx */);
					}
					break;
				default:
					LogError(errno, RS_RET_SYS_ERR, "do_fen: Event from unexpected source "
						": %d\n", portEvent.portev_source);
			}
		}

		DBGPRINTF("do_fen loop end... \n");
	}
	DBGPRINTF("do_fen EXIT monitoring loop \n");

finalize_it:
	/*
	* close port, will de-activate all file events watches associated
	* with the port.
	*/
	close(glport);

	/* Free memory now */
	for(pLstn = runModConf->pRootLstn ; pLstn != NULL ; pLstn = pLstn->next) {
		free(pLstn->pfinf->fobj.fo_name);
		free(pLstn->pfinf);
		pLstn->pfinf = NULL;
	}

	RETiRet;
}
#else /* #if OS_SOLARIS */
static rsRetVal
do_fen(void)
{
	LogError(0, RS_RET_NOT_IMPLEMENTED, "do_fen: mode set to fen, but the "
			"platform does not support fen");
	return RS_RET_NOT_IMPLEMENTED;
}

#endif /* #if OS_SOLARIS */


/* This function is called by the framework to gather the input. The module stays
 * most of its lifetime inside this function. It MUST NEVER exit this function. Doing
 * so would end module processing and rsyslog would NOT reschedule the module. If
 * you exit from this function, you violate the interface specification!
 */
BEGINrunInput
CODESTARTrunInput
	DBGPRINTF("working in %s mode\n",
		 (runModConf->opMode == OPMODE_POLLING) ? "polling" :
			((runModConf->opMode == OPMODE_INOTIFY) ?"inotify" : "fen"));
	if(runModConf->opMode == OPMODE_POLLING)
		iRet = doPolling();
	else if(runModConf->opMode == OPMODE_INOTIFY)
		iRet = do_inotify();
	else if(runModConf->opMode == OPMODE_FEN)
		iRet = do_fen();
	else {
		LogError(0, RS_RET_NOT_IMPLEMENTED, "imfile: unknown mode %d set",
			runModConf->opMode);
		return RS_RET_NOT_IMPLEMENTED;
	}
	DBGPRINTF("terminating upon request of rsyslog core\n");
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

	uchar *const statefn = getStateFileName(pLstn, statefile, sizeof(statefile), NULL);
	DBGPRINTF("persisting state for '%s' to file '%s'\n",
		  pLstn->pszFileName, statefn);
	CHKiRet(strm.Construct(&psSF));
	lenDir = ustrlen(glbl.GetWorkDir());
	if(lenDir > 0)
		CHKiRet(strm.SetDir(psSF, glbl.GetWorkDir(), lenDir));
	CHKiRet(strm.SettOperationsMode(psSF, STREAMMODE_WRITE_TRUNC));
	CHKiRet(strm.SetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetFName(psSF, statefn, strlen((char*) statefn)));
	CHKiRet(strm.SetFileNotFoundError(psSF, pLstn->fileNotFoundError));
	CHKiRet(strm.ConstructFinalize(psSF));

	CHKiRet(strm.Serialize(pLstn->pStrm, psSF));
	CHKiRet(strm.Flush(psSF));

	CHKiRet(strm.Destruct(&psSF));

finalize_it:
	if(psSF != NULL)
		strm.Destruct(&psSF);

	if(iRet != RS_RET_OK) {
		LogError(0, iRet, "imfile: could not persist state "
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
	/* release objects we used */
	objRelease(strm, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);

#	if defined(HAVE_INOTIFY_INIT) || (defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE))
	int i;
	/* we use these vars only in inotify mode */
	if(dirs != NULL) {
		/* Free dirNames */
		for(i = 0 ; i < currMaxDirs ; ++i) {
			free(dirs[i].dirName);
			free(dirs[i].dirNameBfWildCard);
#			if defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)
				free(dirs[i].pfinf->fobj.fo_name);
				free(dirs[i].pfinf);
#			endif
		}
		free(dirs->active.listeners);
		free(dirs->configured.listeners);
		free(dirs);
	}
#	endif /* #if defined(HAVE_INOTIFY_INIT) || (defined(OS_SOLARIS) && defined (HAVE_PORT_SOURCE_FILE)) --- */

#ifdef HAVE_INOTIFY_INIT
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
	LogError(0, NO_ERRCODE, "imfile: ruleset '%s' for %s not found - "
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
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(strm, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));

	DBGPRINTF("version %s initializing\n", VERSION);
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
