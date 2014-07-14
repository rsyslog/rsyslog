/* imfile.c
 * 
 * This is the input module for reading text file data. A text file is a
 * non-binary file who's lines are delemited by the \n character.
 *
 * Work originally begun on 2008-02-01 by Rainer Gerhards
 *
 * Copyright 2008-2014 Adiscon GmbH.
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
#include "datetime.h"
#include "unicode-helper.h"
#include "prop.h"
#include "stringbuf.h"
#include "ruleset.h"
#include "ratelimit.h"

MODULE_TYPE_INPUT	/* must be present for input modules, do not remove */
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imfile")

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA	/* must be present, starts static data */
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime)
DEFobjCurrIf(strm)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)

static int bLegacyCnfModGlobalsPermitted;/* are legacy module-global config parameters permitted? */

#define NUM_MULTISUB 1024 /* default max number of submits */
#define DFLT_PollInterval 10

#define INIT_FILE_TAB_SIZE 4 /* default file table size - is extended as needed, use 2^x value */
#define INIT_FILE_IN_DIR_TAB_SIZE 1 /* initial size for "associated files tab" in directory table */
#define INIT_WDMAP_TAB_SIZE 1 /* default wdMap table size - is extended as needed, use 2^x value */

/* this structure is used in pure polling mode as well one of the support
 * structures for inotify.
 */
typedef struct fileInfo_s {
	uchar *pszFileName;
	uchar *pszDirName;
	uchar *pszBaseName;
	uchar *pszTag;
	size_t lenTag;
	uchar *pszStateFile; /* file in which state between runs is to be stored */
	int iFacility;
	int iSeverity;
	int maxLinesAtOnce;
	int nRecords; /**< How many records did we process before persisting the stream? */
	int iPersistStateInterval; /**< how often should state be persisted? (0=on close only) */
	strm_t *pStrm;	/* its stream (NULL if not assigned) */
	uint8_t readMode;	/* which mode to use in ReadMulteLine call? */
	sbool escapeLF;	/* escape LF inside the MSG content? */
	ruleset_t *pRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	ratelimit_t *ratelimiter;
	multi_submit_t multiSub;
} fileInfo_t;

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
	int maxLinesAtOnce;	/* how many lines to process in a row? */
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
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
	uint8_t readMode;
	sbool escapeLF;
	int maxLinesAtOnce;
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	struct instanceConf_s *next;
};


/* forward definitions */
static rsRetVal persistStrmState(fileInfo_t *pInfo);
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);


#define OPMODE_POLLING 0
#define OPMODE_INOTIFY 1

/* config variables */
struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
	int iPollInterval;	/* number of seconds to sleep when there was no file activity */
	instanceConf_t *root, *tail;
	uint8_t opMode;
	sbool configSetViaV2Method;
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

static int iFilPtr = 0;		/* number of files to be monitored; pointer to next free spot during config */
static fileInfo_t *files = NULL;
static int allocMaxFiles;	/* max file table size currently allocated */

#if HAVE_INOTIFY_INIT
/* support for inotify mode */

/* we need to track directories */
struct dirInfoFiles_s { /* associated files */
	int idx;
	int refcnt;	/* due to inotify's async nature, we may have multiple
			 * references to a single file inside our cache - e.g. when
			 * inodes are removed, and the file name is re-created BUT another
			 * process (like rsyslogd ;)) holds open the old inode.
			 */
};
typedef struct dirInfoFiles_s dirInfoFiles_t;

struct dirInfo_s {
	uchar *dirName;
	dirInfoFiles_t *files;	/* associated file entries */
	int currMaxFiles;
	int allocMaxFiles;
};
typedef struct dirInfo_s dirInfo_t;
static dirInfo_t *dirs = NULL;
static int allocMaxDirs;
static int currMaxDirs;

/* We need to map watch descriptors to our actual objects. Unfortunately, the
 * inotify API does not provide us with any cookie, so a simple O(1) algorithm
 * cannot be done (what a shame...). We assume that maintaining the array is much
 * less often done than looking it up, so we keep the array sorted by watch descriptor
 * and do a binary search on the wd we get back. This is at least O(log n), which
 * is not too bad for the anticipated use case.
 */
struct wd_map_s {
	int wd;		/* ascending sort key */
	int fileIdx;	/* -1, if this is a dir entry, otherwise index into files table */
	int dirIdx;	/* index into dirs table, undefined if fileIdx != -1 */
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
	{ "statefile", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "tag", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "severity", eCmdHdlrSeverity, 0 },
	{ "facility", eCmdHdlrFacility, 0 },
	{ "ruleset", eCmdHdlrString, 0 },
	{ "readmode", eCmdHdlrInt, 0 },
	{ "escapelf", eCmdHdlrBinary, 0 },
	{ "maxlinesatonce", eCmdHdlrInt, 0 },
	{ "maxsubmitatonce", eCmdHdlrInt, 0 },
	{ "persiststateinterval", eCmdHdlrInt, 0 }
};
static struct cnfparamblk inppblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(inppdescr)/sizeof(struct cnfparamdescr),
	  inppdescr
	};

#include "im-helper.h" /* must be included AFTER the type definitions! */


#if HAVE_INOTIFY_INIT
/* support for inotify mode */

#if 0 /* enable if you need this for debugging */
static void
dbg_wdmapPrint(char *msg)
{
	int i;
	dbgprintf("%s\n", msg);
	for(i = 0 ; i < nWdmap ; ++i)
		dbgprintf("wdmap[%d]: wd: %d, file %d, dir %d\n", i,
			  wdmap[i].wd, wdmap[i].fileIdx, wdmap[i].dirIdx);
}
#endif

static inline rsRetVal
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
wdmapAdd(int wd, int dirIdx, int fileIdx)
{
	wd_map_t *newmap;
	int newmapsize;
	int i;
	DEFiRet;

	for(i = nWdmap-1 ; i >= 0 && wdmap[i].wd > wd ; --i)
		; 	/* just scan */
	if(i >= 0 && wdmap[i].wd == wd) {
		DBGPRINTF("imfile: wd %d already in wdmap!\n", wd);
		FINALIZE;
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
		dbgprintf("DDDD: imfile doing wdmap mmemmov(%d, %d, %d) for ADD\n", i,i+1,nWdmap-i);
		memmove(wdmap + i, wdmap + i + 1, nWdmap - i);
	}
	wdmap[i].wd = wd;
	wdmap[i].dirIdx = dirIdx;
	wdmap[i].fileIdx = fileIdx;
	++nWdmap;
	dbgprintf("DDDD: imfile: enter into wdmap[%d]: wd %d, dir %d, file %d\n",i,wd,dirIdx,fileIdx);

finalize_it:
	RETiRet;
}

static rsRetVal
wdmapDel(int wd)
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
		dbgprintf("DDDD: imfile doing wdmap mmemmov(%d, %d, %d) for DEL\n", i,i+1,nWdmap-i-1);
		memmove(wdmap + i, wdmap + i+1, nWdmap - i-1);
	}
	--nWdmap;
	dbgprintf("DDDD: imfile: wd %d deleted, was idx %d\n", wd, i);

finalize_it:
	RETiRet;
}

#endif /* #if HAVE_INOTIFY_INIT */


/* enqueue the read file line as a message. The provided string is
 * not freed - thuis must be done by the caller.
 */
static rsRetVal enqLine(fileInfo_t *pInfo, cstr_t *cstrLine)
{
	DEFiRet;
	msg_t *pMsg;

	if(rsCStrLen(cstrLine) == 0) {
		/* we do not process empty lines */
		FINALIZE;
	}

	CHKiRet(msgConstruct(&pMsg));
	MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
	MsgSetInputName(pMsg, pInputName);
	MsgSetRawMsg(pMsg, (char*)rsCStrGetSzStr(cstrLine), cstrLen(cstrLine));
	MsgSetMSGoffs(pMsg, 0);	/* we do not have a header... */
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetTAG(pMsg, pInfo->pszTag, pInfo->lenTag);
	pMsg->iFacility = LOG_FAC(pInfo->iFacility);
	pMsg->iSeverity = LOG_PRI(pInfo->iSeverity);
	MsgSetRuleset(pMsg, pInfo->pRuleset);
	ratelimitAddMsg(pInfo->ratelimiter, &pInfo->multiSub, pMsg);
finalize_it:
	RETiRet;
}


/* try to open a file. This involves checking if there is a status file and,
 * if so, reading it in. Processing continues from the last know location.
 */
static rsRetVal
openFile(fileInfo_t *pThis)
{
	DEFiRet;
	strm_t *psSF = NULL;
	uchar pszSFNam[MAXFNAME];
	size_t lenSFNam;
	struct stat stat_buf;

	/* Construct file name */
	lenSFNam = snprintf((char*)pszSFNam, sizeof(pszSFNam) / sizeof(uchar), "%s/%s",
			     (char*) glbl.GetWorkDir(), (char*)pThis->pszStateFile);

	/* check if the file exists */
	if(stat((char*) pszSFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			dbgprintf("filemon %p: clean startup, no .si file found\n", pThis);
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			dbgprintf("filemon %p: error %d trying to access .si file\n", pThis, errno);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}

	/* If we reach this point, we have a .si file */

	CHKiRet(strm.Construct(&psSF));
	CHKiRet(strm.SettOperationsMode(psSF, STREAMMODE_READ));
	CHKiRet(strm.SetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetFName(psSF, pszSFNam, lenSFNam));
	CHKiRet(strm.ConstructFinalize(psSF));

	/* read back in the object */
	CHKiRet(obj.Deserialize(&pThis->pStrm, (uchar*) "strm", psSF, NULL, pThis));

	strm.CheckFileChange(pThis->pStrm);
	CHKiRet(strm.SeekCurrOffs(pThis->pStrm));

	/* note: we do not delete the state file, so that the last position remains
	 * known even in the case that rsyslogd aborts for some reason (like powerfail)
	 */

finalize_it:
	if(psSF != NULL)
		strm.Destruct(&psSF);

	if(iRet != RS_RET_OK) {
		if(pThis->pStrm != NULL)
			strm.Destruct(&pThis->pStrm);
		CHKiRet(strm.Construct(&pThis->pStrm));
		CHKiRet(strm.SettOperationsMode(pThis->pStrm, STREAMMODE_READ));
		CHKiRet(strm.SetsType(pThis->pStrm, STREAMTYPE_FILE_MONITOR));
		CHKiRet(strm.SetFName(pThis->pStrm, pThis->pszFileName, strlen((char*) pThis->pszFileName)));
		CHKiRet(strm.ConstructFinalize(pThis->pStrm));
	}

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
#pragma GCC diagnostic ignored "-Wempty-body"
static rsRetVal pollFile(fileInfo_t *pThis, int *pbHadFileData)
{
	cstr_t *pCStr = NULL;
	int nProcessed = 0;
	DEFiRet;

	/* Note: we must do pthread_cleanup_push() immediately, because the POXIS macros
	 * otherwise do not work if I include the _cleanup_pop() inside an if... -- rgerhards, 2008-08-14
	 */
	pthread_cleanup_push(pollFileCancelCleanup, &pCStr);
	if(pThis->pStrm == NULL) {
		CHKiRet(openFile(pThis)); /* open file */
	}

	/* loop below will be exited when strmReadLine() returns EOF */
	while(glbl.GetGlobalInputTermState() == 0) {
		if(pThis->maxLinesAtOnce != 0 && nProcessed >= pThis->maxLinesAtOnce)
			break;
		CHKiRet(strm.ReadLine(pThis->pStrm, &pCStr, pThis->readMode, pThis->escapeLF));
		++nProcessed;
		if(pbHadFileData != NULL)
			*pbHadFileData = 1; /* this is just a flag, so set it and forget it */
		CHKiRet(enqLine(pThis, pCStr)); /* process line */
		rsCStrDestruct(&pCStr); /* discard string (must be done by us!) */
		if(pThis->iPersistStateInterval > 0 && pThis->nRecords++ >= pThis->iPersistStateInterval) {
			persistStrmState(pThis);
			pThis->nRecords = 0;
		}
	}

finalize_it:
	multiSubmitFlush(&pThis->multiSub);
	pthread_cleanup_pop(0);

	if(pCStr != NULL) {
		rsCStrDestruct(&pCStr);
	}

	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* create input instance, set default paramters, and
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
	inst->maxLinesAtOnce = 10240;
	inst->iPersistStateInterval = 0;
	inst->readMode = 0;
	inst->escapeLF = 1;

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


/* this function checks instance parameters and does some required pre-processing
 * (e.g. split filename in path and actual name)
 * Note: we do NOT use dirname()/basename() as they have portability problems.
 */
static rsRetVal
checkInstance(instanceConf_t *inst)
{
	char dirn[MAXFNAME];
	char basen[MAXFNAME];
	int i;
	int lenName;
	struct stat sb;
	int r;
	int eno;
	char errStr[512];
	DEFiRet;

	lenName = ustrlen(inst->pszFileName);
	for(i = lenName ; i >= 0 ; --i) {
		if(inst->pszFileName[i] == '/') {
			/* found basename component */
			if(i == lenName)
				basen[0] = '\0';
			else {
				memcpy(basen, inst->pszFileName+i+1, lenName-i);
				/* Note \0 is copied above! */
				//basen[(lenName-i+1)+1] = '\0';
			}
			break;
		}
	}
	memcpy(dirn, inst->pszFileName, i); /* do not copy slash */
	dirn[i] = '\0';
	CHKmalloc(inst->pszFileBaseName = (uchar*) strdup(basen));
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
static rsRetVal addInstance(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	instanceConf_t *inst;
	DEFiRet;

	if(cs.pszFileName == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no file name given, file monitor can not be created");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(cs.pszFileTag == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no tag value given , file monitor can not be created");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(cs.pszStateFile == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: not state file name given, file monitor can not be created");
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
	inst->pszStateFile = (uchar*) strdup((char*) cs.pszStateFile);
	inst->iSeverity = cs.iSeverity;
	inst->iFacility = cs.iFacility;
	inst->maxLinesAtOnce = cs.maxLinesAtOnce;
	inst->iPersistStateInterval = cs.iPersistStateInterval;
	inst->readMode = cs.readMode;
	inst->escapeLF = 0;

	CHKiRet(checkInstance(inst));

	/* reset legacy system */
	cs.iPersistStateInterval = 0;
	resetConfigVariables(NULL, NULL); /* values are both dummies */

finalize_it:
	free(pNewVal); /* we do not need it, but we must free it! */
	RETiRet;
}


/* This function is called when a new listener (monitor) shall be added. */
static inline rsRetVal
addListner(instanceConf_t *inst)
{
	DEFiRet;
	int newMax;
	fileInfo_t *newFileTab;
	fileInfo_t *pThis;

	if(iFilPtr == allocMaxFiles) {
		newMax = 2 * allocMaxFiles;
		newFileTab = realloc(files, newMax * sizeof(fileInfo_t));
		if(newFileTab == NULL) {
			errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
					"cannot alloc memory to monitor file '%s' - ignoring",
					inst->pszFileName);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		files = newFileTab;
		allocMaxFiles = newMax;
		DBGPRINTF("imfile: increased file table to %d entries\n", allocMaxFiles);
	}

	/* if we reach this point, there is space in the file table for the new entry */
	pThis = &files[iFilPtr];
	pThis->pszFileName = (uchar*) strdup((char*) inst->pszFileName);
	pThis->pszDirName = inst->pszDirName; /* use value from inst! */
	pThis->pszBaseName = inst->pszFileBaseName; /* use value from inst! */
	pThis->pszTag = (uchar*) strdup((char*) inst->pszTag);
	pThis->lenTag = ustrlen(pThis->pszTag);
	pThis->pszStateFile = (uchar*) strdup((char*) inst->pszStateFile);

	CHKiRet(ratelimitNew(&pThis->ratelimiter, "imfile", (char*)inst->pszFileName));
	CHKmalloc(pThis->multiSub.ppMsgs = MALLOC(inst->nMultiSub * sizeof(msg_t*)));
	pThis->multiSub.maxElem = inst->nMultiSub;
	pThis->multiSub.nElem = 0;
	pThis->iSeverity = inst->iSeverity;
	pThis->iFacility = inst->iFacility;
	pThis->maxLinesAtOnce = inst->maxLinesAtOnce;
	pThis->iPersistStateInterval = inst->iPersistStateInterval;
	pThis->readMode = inst->readMode;
	pThis->escapeLF = inst->escapeLF;
	pThis->pRuleset = inst->pBindRuleset;
	pThis->nRecords = 0;
	pThis->pStrm = NULL;
	++iFilPtr;	/* we got a new file to monitor */

	resetConfigVariables(NULL, NULL); /* values are both dummies */
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
		dbgprintf("input param blk in imfile:\n");
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
		} else if(!strcmp(inppblk.descr[i].name, "tag")) {
			inst->pszTag = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "severity")) {
			inst->iSeverity = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "facility")) {
			inst->iFacility = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "readmode")) {
			inst->readMode = (uint8_t) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "escapelf")) {
			inst->escapeLF = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "maxlinesatonce")) {
			inst->maxLinesAtOnce = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "persiststateinterval")) {
			inst->iPersistStateInterval = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "maxsubmitatonce")) {
			inst->nMultiSub = pvals[i].val.d.n;
		} else {
			dbgprintf("imfile: program error, non-handled "
			  "param '%s'\n", inppblk.descr[i].name);
		}
	}
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
	cs.pBindRuleset = NULL;
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
		dbgprintf("module (global) param blk for imfile:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "pollinginterval")) {
			loadModConf->iPollInterval = (int) pvals[i].val.d.n;
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
			dbgprintf("imfile: program error, non-handled "
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
	dbgprintf("imfile: opmode is %d, polling interval is %d\n",
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
	free(files); /* clear any previous instance */
	CHKmalloc(files = (fileInfo_t*) malloc(sizeof(fileInfo_t) * INIT_FILE_TAB_SIZE));
	allocMaxFiles = INIT_FILE_TAB_SIZE;
	iFilPtr = 0;

	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		addListner(inst);
	}

	/* if we could not set up any listeners, there is no point in running... */
	if(iFilPtr == 0) {
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
		del = inst;
		inst = inst->next;
		free(del);
	}
	free(files);
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
	int i;
	int bHadFileData; /* were there at least one file with data during this run? */
	DEFiRet;
	while(glbl.GetGlobalInputTermState() == 0) {
		do {
			bHadFileData = 0;
			for(i = 0 ; i < iFilPtr ; ++i) {
				if(glbl.GetGlobalInputTermState() == 1)
					break; /* terminate input! */
				pollFile(&files[i], &bHadFileData);
			}
		} while(iFilPtr > 1 && bHadFileData == 1 && glbl.GetGlobalInputTermState() == 0);
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


#if HAVE_INOTIFY_INIT
/* add entry to dirs array */
static rsRetVal
dirsAdd(uchar *dirName)
{
	int newMax;
	dirInfo_t *newDirTab;
	DEFiRet;

	if(currMaxDirs == allocMaxDirs) {
		newMax = 2 * allocMaxDirs;
		newDirTab = realloc(dirs, newMax * sizeof(dirInfo_t));
		if(newDirTab == NULL) {
			errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
					"cannot alloc memory to monitor directory '%s' - ignoring",
					dirName);
		}
		dirs = newDirTab;
		allocMaxDirs = newMax;
		DBGPRINTF("imfile: increased dir table to %d entries\n", allocMaxDirs);
	}

	/* if we reach this point, there is space in the file table for the new entry */
	dirs[currMaxDirs].dirName = dirName;
	CHKmalloc(dirs[currMaxDirs].files= malloc(sizeof(dirInfoFiles_t) * INIT_FILE_IN_DIR_TAB_SIZE));
	dirs[currMaxDirs].allocMaxFiles = INIT_FILE_IN_DIR_TAB_SIZE;
	dirs[currMaxDirs].currMaxFiles= 0;

	++currMaxDirs;
finalize_it:
	RETiRet;
}

/* checks if a file name is already inside the dirs array. Note that wildcards
 * apply. Returns either the array index or -1 if not found.
 * i is the index of the dir entry to search.
 */
static int
dirsFindFile(int i, uchar *fn)
{
	int f;
	uchar *baseName;

	for(f = 0 ; f < dirs[i].currMaxFiles ; ++f) {
		baseName = files[dirs[i].files[f].idx].pszBaseName;
		if(!fnmatch((char*)fn, (char*)baseName, FNM_PATHNAME | FNM_PERIOD))
			break; /* found */
	}
	if(f == dirs[i].currMaxFiles)
		f = -1;
	//dbgprintf("DDDD: dir '%s', file '%s', found:%d\n", dirs[i].dirName, fn, f);
	return f;
}

/* checks if a dir name is already inside the dirs array. If so, returns
 * its index. If not present, -1 is returned.
 */
static int
dirsFindDir(uchar *dir)
{
	int i;

	for(i = 0 ; i < currMaxDirs && ustrcmp(dir, dirs[i].dirName) ; ++i)
		; /* just scan, all done in for() */
	if(i == currMaxDirs)
		i = -1;
	//dbgprintf("DDDD: dir '%s', found:%d\n", dir, i);
	return i;
}

static rsRetVal
dirsInit(void)
{
	instanceConf_t *inst;
	DEFiRet;

	free(dirs);
	CHKmalloc(dirs = malloc(sizeof(dirInfo_t) * INIT_FILE_TAB_SIZE));
	allocMaxDirs = INIT_FILE_TAB_SIZE;
	currMaxDirs = 0;

	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		if(dirsFindDir(inst->pszDirName) == -1)
			dirsAdd(inst->pszDirName);
	}

finalize_it:
	RETiRet;
}

/* add file to directory (create association)
 * i is index into file table, all other information is pulled from that table.
 */
static rsRetVal
dirsAddFile(int i)
{
	int dirIdx;
	int j;
	int newMax;
	dirInfoFiles_t *newFileTab;
	dirInfo_t *dir;
	DEFiRet;

	dirIdx = dirsFindDir(files[i].pszDirName);
	if(dirIdx == -1) {
		errmsg.LogError(0, RS_RET_INTERNAL_ERROR, "imfile: could not find "
			"directory '%s' in dirs array - ignoring",
			files[i].pszDirName);
		FINALIZE;
	}

	dir = dirs + dirIdx;
	for(j = 0 ; j < dir->currMaxFiles && dir->files[j].idx != i ; ++j)
		; /* just scan */
	if(j < dir->currMaxFiles) {
		/* this is not important enough to send an user error, as all will
		 * continue to work. */
		++dir->files[j].refcnt;
		DBGPRINTF("imfile: file '%s' already registered in directory '%s', recnt now %d\n",
			files[i].pszFileName, dir->dirName, dir->files[j].refcnt);
		FINALIZE;
	}

	if(dir->currMaxFiles == dir->allocMaxFiles) {
		newMax = 2 * allocMaxFiles;
		newFileTab = realloc(dirs, newMax * sizeof(dirInfoFiles_t));
		if(newFileTab == NULL) {
			errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
					"cannot alloc memory to map directory '%s' file relationship "
					"'%s' - ignoring", files[i].pszFileName, dir->dirName);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		dir->files = newFileTab;
		dir->allocMaxFiles = newMax;
		DBGPRINTF("imfile: increased dir table to %d entries\n", allocMaxDirs);
	}

	dir->files[dir->currMaxFiles].idx = i;
	dir->files[dir->currMaxFiles].refcnt = 1;
	dbgprintf("DDDD: associated file %d[%s] to directory %d[%s]\n",
		i, files[i].pszFileName, dirIdx, dir->dirName);
	++dir->currMaxFiles;
finalize_it:
	RETiRet;
}

/* delete a file from directory (remove association) 
 * fIdx is index into file table, all other information is pulled from that table.
 */
static rsRetVal
dirsDelFile(int fIdx)
{
	int dirIdx;
	int j;
	dirInfo_t *dir;
	DEFiRet;

	dirIdx = dirsFindDir(files[fIdx].pszDirName);
	if(dirIdx == -1) {
		DBGPRINTF("imfile: could not find directory '%s' in dirs array - ignoring",
			files[fIdx].pszDirName);
		FINALIZE;
	}

	dir = dirs + dirIdx;
	for(j = 0 ; j < dir->currMaxFiles && dir->files[j].idx != fIdx ; ++j)
		; /* just scan */
	if(j == dir->currMaxFiles) {
		DBGPRINTF("imfile: no association for file '%s' in directory '%s' "
			"found - ignoring\n", files[fIdx].pszFileName, dir->dirName);
		FINALIZE;
	}
	dir->files[j].refcnt--;
	if(dir->files[j].refcnt == 0) {
		/* we remove that entry (but we never shrink the table) */
		if(j < dir->currMaxFiles - 1) {
			/* entry in middle - need to move others */
			memmove(dir->files+j, dir->files+j+1,
				(dir->currMaxFiles -j-1) * sizeof(dirInfoFiles_t));
		}
		--dir->currMaxFiles;
	}
	DBGPRINTF("imfile: removed association of file '%s' to directory '%s'\n",
		  files[fIdx].pszFileName, dir->dirName);

finalize_it:
	RETiRet;
}

static void
in_setupDirWatch(int i)
{
	int wd;
	wd = inotify_add_watch(ino_fd, (char*)dirs[i].dirName, IN_CREATE);
	if(wd < 0) {
		DBGPRINTF("imfile: could not create dir watch for '%s'\n",
			files[i].pszFileName);
		goto done;
	}
	wdmapAdd(wd, i, -1);
	dbgprintf("DDDD: watch %d added for dir %s\n", wd, dirs[i].dirName);
done:	return;
}

/* Setup a new file watch.
 * Note: we need to try to read this file, as it may already contain data this
 * needs to be processed, and we won't get an event for that as notifications
 * happen only for things after the watch has been activated.
 */
static void
in_setupFileWatch(int i)
{
	int wd;
	wd = inotify_add_watch(ino_fd, (char*)files[i].pszFileName, IN_MODIFY);
	if(wd < 0) {
		DBGPRINTF("imfile: could not create initial file for '%s'\n",
			files[i].pszFileName);
		goto done;
	}
	wdmapAdd(wd, -1, i);
	dbgprintf("DDDD: watch %d added for file %s\n", wd, files[i].pszFileName);
	dirsAddFile(i);
	pollFile(&files[i], NULL);
done:	return;
}

/* setup our initial set of watches, based on user config */
static rsRetVal
in_setupInitialWatches()
{
	int i;
	DEFiRet;

	for(i = 0 ; i < currMaxDirs ; ++i) {
		in_setupDirWatch(i);
	}
	for(i = 0 ; i < iFilPtr ; ++i) {
		in_setupFileWatch(i);
	}
	RETiRet;
}

static void
in_dbg_showEv(struct inotify_event *ev)
{
	if(ev->mask & IN_IGNORED) {
		dbgprintf("watch was REMOVED\n");
	} else if(ev->mask & IN_MODIFY) {
		dbgprintf("watch was MODIFID\n");
	} else if(ev->mask & IN_ACCESS) {
		dbgprintf("watch IN_ACCESS\n");
	} else if(ev->mask & IN_ATTRIB) {
		dbgprintf("watch IN_ATTRIB\n");
	} else if(ev->mask & IN_CLOSE_WRITE) {
		dbgprintf("watch IN_CLOSE_WRITE\n");
	} else if(ev->mask & IN_CLOSE_NOWRITE) {
		dbgprintf("watch IN_CLOSE_NOWRITE\n");
	} else if(ev->mask & IN_CREATE) {
		dbgprintf("file was CREATED: %s\n", ev->name);
	} else if(ev->mask & IN_DELETE) {
		dbgprintf("watch IN_DELETE\n");
	} else if(ev->mask & IN_DELETE_SELF) {
		dbgprintf("watch IN_DELETE_SELF\n");
	} else if(ev->mask & IN_MOVE_SELF) {
		dbgprintf("watch IN_MOVE_SELF\n");
	} else if(ev->mask & IN_MOVED_FROM) {
		dbgprintf("watch IN_MOVED_FROM\n");
	} else if(ev->mask & IN_MOVED_TO) {
		dbgprintf("watch IN_MOVED_TO\n");
	} else if(ev->mask & IN_OPEN) {
		dbgprintf("watch IN_OPEN\n");
	} else if(ev->mask & IN_ISDIR) {
		dbgprintf("watch IN_ISDIR\n");
	} else {
		dbgprintf("unknown mask code %8.8x\n", ev->mask);
	 }
}

static void
in_handleDirEvent(struct inotify_event *ev, int dirIdx)
{
	int fileIdx;
	dbgprintf("DDDD: handle dir event for %s\n", dirs[dirIdx].dirName);
	if(!(ev->mask & IN_CREATE)) {
		DBGPRINTF("imfile: got non-expected inotify event:\n");
		in_dbg_showEv(ev);
		goto done;
	}
	fileIdx = dirsFindFile(dirIdx, (uchar*)ev->name);
	if(fileIdx == -1) {
		dbgprintf("imfile: file '%s' not associated with dir '%s'\n",
			ev->name, dirs[dirIdx].dirName);
		goto done;
	}
	dbgprintf("DDDD: file '%s' associated with dir '%s'\n", ev->name, dirs[dirIdx].dirName);
	in_setupFileWatch(fileIdx);
done:	return;
}

/* inotify told us that a file's wd was closed. We now need to remove
 * the file from our internal structures. Remember that a different inode
 * with the same name may already be in processing.
 */
static void
in_removeFile(struct inotify_event *ev, int fIdx)
{
	wdmapDel(ev->wd);
	dirsDelFile(fIdx);
}


static void
in_handleFileEvent(struct inotify_event *ev, int fIdx)
{
	if(ev->mask & IN_MODIFY) {
		pollFile(&files[fIdx], NULL);
	} else if(ev->mask & IN_IGNORED) {
		in_removeFile(ev, fIdx);
	} else {
		DBGPRINTF("imfile: got non-expected inotify event:\n");
		in_dbg_showEv(ev);
	}
}

static void
in_processEvent(struct inotify_event *ev)
{
	wd_map_t *etry;

	etry =  wdmapLookup(ev->wd);
	if(etry == NULL) {
		DBGPRINTF("imfile: could not lookup wd %d\n", ev->wd);
		goto done;
	}
	dbgprintf("DDDD: imfile: wd %d got file %d, dir %d\n", ev->wd, etry->fileIdx, etry->dirIdx);
	if(etry->fileIdx == -1) { /* directory? */
		in_handleDirEvent(ev, etry->dirIdx);
	} else {
		in_handleFileEvent(ev, etry->fileIdx);
	}
done:	return;
}

/* Monitor files in inotify mode */
static rsRetVal
do_inotify()
{
	char iobuf[8192];
	struct inotify_event *ev;
	int rd;
	int currev;
	DEFiRet;

	CHKiRet(wdmapInit());
	CHKiRet(dirsInit());
	ino_fd = inotify_init();
	DBGPRINTF("imfile: inotify fd %d\n", ino_fd);
	CHKiRet(in_setupInitialWatches());

	while(glbl.GetGlobalInputTermState() == 0) {
		rd = read(ino_fd, iobuf, sizeof(iobuf));
		if(rd < 0) {
			perror("inotify read"); exit(1);
		}
		currev = 0;
		while(currev < rd) {
			ev = (struct inotify_event*) (iobuf+currev);
			dbgprintf("DDDD: imfile event notification: rd %d[%d], wd (%d, mask "
				"%8.8x, cookie %4.4x, len %d)\n",
				(int) rd, currev, ev->wd, ev->mask, ev->cookie, ev->len);
			in_dbg_showEv(ev);
			in_processEvent(ev);
			currev += sizeof(struct inotify_event) + ev->len;
		}
	}

finalize_it:
	close(ino_fd);
	RETiRet;
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
persistStrmState(fileInfo_t *pInfo)
{
	DEFiRet;
	strm_t *psSF = NULL; /* state file (stream) */
	size_t lenDir;

	ASSERT(pInfo != NULL);

	/* TODO: create a function persistObj in obj.c? */
	CHKiRet(strm.Construct(&psSF));
	lenDir = ustrlen(glbl.GetWorkDir());
	if(lenDir > 0)
		CHKiRet(strm.SetDir(psSF, glbl.GetWorkDir(), lenDir));
	CHKiRet(strm.SettOperationsMode(psSF, STREAMMODE_WRITE_TRUNC));
	CHKiRet(strm.SetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetFName(psSF, pInfo->pszStateFile, strlen((char*) pInfo->pszStateFile)));
	CHKiRet(strm.ConstructFinalize(psSF));

	CHKiRet(strm.Serialize(pInfo->pStrm, psSF));
	CHKiRet(strm.Flush(psSF));

	CHKiRet(strm.Destruct(&psSF));

finalize_it:
	if(psSF != NULL)
		strm.Destruct(&psSF);
	
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "imfile: could not persist state "
				"file %s - data may be repeated on next "
				"startup. Is WorkDirectory set?",
				pInfo->pszStateFile);
	}

	RETiRet;
}


/* This function is called by the framework after runInput() has been terminated. It
 * shall free any resources and prepare the module for unload.
 */
BEGINafterRun
	int i;
CODESTARTafterRun
	/* Close files and persist file state information. We do NOT abort on error iRet as that makes
	 * matters worse (at least we can try persisting the others...). Please note that, under stress
	 * conditions, it may happen that we are terminated before we actuall could open all streams. So
	 * before we change anything, we need to make sure the stream was open.
	 */
	for(i = 0 ; i < iFilPtr ; ++i) {
		if(files[i].pStrm != NULL) { /* stream open? */
			persistStrmState(&files[i]);
			strm.Destruct(&(files[i].pStrm));
		}
		ratelimitDestruct(files[i].ratelimiter);
		free(files[i].multiSub.ppMsgs);
		free(files[i].pszFileName);
		free(files[i].pszTag);
		free(files[i].pszStateFile);
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
	objRelease(datetime, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
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
	cs.pBindRuleset = NULL;
	cs.maxLinesAtOnce = 10240;

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
	CHKiRet(objUse(datetime, CORE_COMPONENT));
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
