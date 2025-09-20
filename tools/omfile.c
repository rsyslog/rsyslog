/* omfile.c
 * This is the implementation of the build-in file output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 * works!
 *
 * File begun on 2007-07-21 by RGerhards (extracted from syslogd.c, which
 * at the time of the fork from sysklogd was under BSD license)
 *
 * A large re-write of this file was done in June, 2009. The focus was
 * to introduce many more features (like zipped writing), clean up the code
 * and make it more reliable. In short, that rewrite tries to provide a new
 * solid basis for the next three to five years to come. During it, bugs
 * may have been introduced ;) -- rgerhards, 2009-06-04
 *
 * Note that as of 2010-02-28 this module does no longer handle
 * pipes. These have been moved to ompipe, to reduced the entanglement
 * between the two different functionalities. -- rgerhards
 *
 * Copyright 2007-2024 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * -or-
 * see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#ifdef HAVE_ATOMIC_BUILTINS
    #include <pthread.h>
#endif

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "outchannel.h"
#include "omfile.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"
#include "stream.h"
#include "unicode-helper.h"
#include "atomic.h"
#include "statsobj.h"
#include "sigprov.h"
#include "cryprov.h"
#include "parserif.h"
#include "janitor.h"
#include "rsconf.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omfile")

/* forward definitions */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* internal structures
 */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(strm) DEFobjCurrIf(statsobj)

/**
 * @brief Global counter/clock for file access timestamping.
 *
 * This counter is used as a "Lamport logical clock" for the LRU mechanism
 * of the dynamic file cache. It provides a monotonically increasing sequence
 * number to track the access order of files, rather than actual time.
 * It is highly unlikely to wrap within the lifetime of a process.
 */
#ifdef HAVE_ATOMIC_BUILTINS64
    static uint64 clockFileAccess = 0;
#else
    static unsigned clockFileAccess = 0;
#endif

/**
 * @brief Mutex for protecting clockFileAccess in non-atomic builtins environments.
 */
#ifndef HAVE_ATOMIC_BUILTINS
static pthread_mutex_t mutClock;
#endif

/**
 * @brief Retrieves and increments the global file access clock.
 *
 * This function provides a monotonically increasing counter used for the LRU
 * mechanism in the dynamic file cache. It is thread-safe.
 *
 * @return The incremented value of the file access clock.
 */
static uint64 getClockFileAccess(void) {
#ifdef HAVE_ATOMIC_BUILTINS64
    return ATOMIC_INC_AND_FETCH_uint64(&clockFileAccess, &mutClock);
#else
    return ATOMIC_INC_AND_FETCH_unsigned(&clockFileAccess, &mutClock);
#endif
}


/**
 * @brief Structure for a dynamic file name cache entry.
 *
 * This structure holds information about a dynamically opened file,
 * including its name, the associated stream, signature provider data,
 * and access timestamp for LRU management.
 */
struct s_dynaFileCacheEntry {
    uchar *pName; /**< name currently open, if dynamic name */
    strm_t *pStrm; /**< our output stream */
    void *sigprovFileData; /**< opaque data ptr for provider use */
    uint64 clkTickAccessed; /**< for LRU - based on clockFileAccess */
    short nInactive; /**< number of minutes not writen - for close timeout */
};
typedef struct s_dynaFileCacheEntry dynaFileCacheEntry;


#define IOBUF_DFLT_SIZE 4096 /**< default size for io buffers */
#define FLUSH_INTRVL_DFLT 1 /**< default buffer flush interval (in seconds) */
#define USE_ASYNCWRITER_DFLT 0 /**< default buffer use async writer */
#define FLUSHONTX_DFLT 1 /**< default for flush on TX end */
#define ADDLF_SIGBUF_STACKLEN 1024 /**< stack buffer size for addLF signing helper */

/**
 * @brief Instance data for the omfile module.
 *
 * This structure holds the configuration and runtime state for each
 * file output action.
 */
typedef struct _instanceData {
    pthread_mutex_t mutWrite; /**< guard against multiple instances writing to single file */
    uchar *fname; /**< file or template name (display only) */
    uchar *tplName; /**< name of assigned template */
    strm_t *pStrm; /**< our output stream */
    short nInactive; /**< number of minutes not writen (STATIC files only) */
    char bDynamicName; /**< 0 - static name, 1 - dynamic name (with properties) */
    int isDevNull; /**< do we "write" to /dev/null? - if so, do nothing */
    int fCreateMode; /**< file creation mode for open() */
    int fDirCreateMode; /**< creation mode for mkdir() */
    int bCreateDirs; /**< auto-create directories? */
    int bSyncFile; /**< should the file by sync()'ed? 1- yes, 0- no */
    uint8_t iNumTpls; /**< number of tpls we use */
    uid_t fileUID; /**< IDs for creation */
    uid_t dirUID;
    gid_t fileGID;
    gid_t dirGID;
    int bFailOnChown; /**< fail creation if chown fails? */
    uchar *sigprovName; /**< signature provider */
    uchar *sigprovNameFull; /**< full internal signature provider name */
    sigprov_if_t sigprov; /**< ptr to signature provider interface */
    void *sigprovData; /**< opaque data ptr for provider use */
    void *sigprovFileData; /**< opaque data ptr for file instance */
    sbool useSigprov; /**< quicker than checkig ptr (1 vs 8 bytes!) */
    uchar *cryprovName; /**< crypto provider */
    uchar *cryprovNameFull; /**< full internal crypto provider name */
    void *cryprovData; /**< opaque data ptr for provider use */
    cryprov_if_t cryprov; /**< ptr to crypto provider interface */
    sbool useCryprov; /**< quicker than checkig ptr (1 vs 8 bytes!) */
    int iCurrElt; /**< currently active cache element (-1 = none) */
    int iCurrCacheSize; /**< currently cache size (1-based) */
    int iDynaFileCacheSize; /**< size of file handle cache */
    /**
     * The cache is implemented as an array. An empty element is indicated
     * by a NULL pointer. Memory is allocated as needed. The following
     * pointer points to the overall structure.
     */
    dynaFileCacheEntry **dynCache;
    off_t iSizeLimit; /**< file size limit, 0 = no limit */
    uchar *pszSizeLimitCmd; /**< command to carry out when size limit is reached */
    int iZipLevel; /**< zip mode to use for this selector */
    int iIOBufSize; /**< size of associated io buffer */
    int iFlushInterval; /**< how fast flush buffer on inactivity? */
    short iCloseTimeout; /**< after how many *minutes* shall the file be closed if inactive? */
    sbool bFlushOnTXEnd; /**< flush write buffers when transaction has ended? */
    sbool bUseAsyncWriter; /**< use async stream writer? */
    sbool bVeryRobustZip;
    sbool bAddLF; /**< append LF to records that are missing it? */
    statsobj_t *stats; /**< dynafile, primarily cache stats */
    STATSCOUNTER_DEF(ctrRequests, mutCtrRequests);
    STATSCOUNTER_DEF(ctrLevel0, mutCtrLevel0);
    STATSCOUNTER_DEF(ctrEvict, mutCtrEvict);
    STATSCOUNTER_DEF(ctrMiss, mutCtrMiss);
    STATSCOUNTER_DEF(ctrMax, mutCtrMax);
    STATSCOUNTER_DEF(ctrCloseTimeouts, mutCtrCloseTimeouts);
    char janitorID[128]; /**< holds ID for janitor calls */
} instanceData;

/**
 * @brief Worker instance data for the omfile module.
 *
 * This structure holds a pointer to the main instance data for use
 * by worker threads.
 */
typedef struct wrkrInstanceData {
    instanceData *pData;
} wrkrInstanceData_t;

/**
 * @brief Module-global configuration settings.
 *
 * This structure holds the configuration settings that apply globally
 * to the omfile module.
 */
typedef struct configSettings_s {
    int iDynaFileCacheSize; /**< max cache for dynamic files */
    int fCreateMode; /**< mode to use when creating files */
    int fDirCreateMode; /**< mode to use when creating files */
    int bFailOnChown; /**< fail if chown fails? */
    uid_t fileUID; /**< UID to be used for newly created files */
    uid_t fileGID; /**< GID to be used for newly created files */
    uid_t dirUID; /**< UID to be used for newly created directories */
    uid_t dirGID; /**< GID to be used for newly created directories */
    int bCreateDirs; /**< auto-create directories for dynaFiles: 0 - no, 1 - yes */
    int bEnableSync; /**< enable syncing of files (no dash in front of pathname in conf): 0 - no, 1 - yes */
    int iZipLevel; /**< zip compression mode (0..9 as usual) */
    sbool bFlushOnTXEnd; /**< flush write buffers when transaction has ended? */
    int64 iIOBufSize; /**< size of an io buffer */
    int iFlushInterval; /**< how often flush the output buffer on inactivity? */
    int bUseAsyncWriter; /**< should we enable asynchronous writing? */
    sbool bAddLF; /**< append LF to records that are missing it? */
    EMPTY_STRUCT
} configSettings_t;
static configSettings_t cs;
uchar *pszFileDfltTplName; /**< name of the default template to use */

/**
 * @brief Module configuration data for the v6 config system.
 */
struct modConfData_s {
    rsconf_t *pConf; /**< our overall config object */
    uchar *tplName; /**< default template */
    int fCreateMode; /**< default mode to use when creating files */
    int fDirCreateMode; /**< default mode to use when creating files */
    uid_t fileUID; /**< default IDs for creation */
    uid_t dirUID;
    gid_t fileGID;
    gid_t dirGID;
    int bDynafileDoNotSuspend;
    strm_compressionDriver_t compressionDriver;
    int compressionDriver_workers;
    sbool addLF; /**< default setting for addLF action parameter */
};

static modConfData_t *loadModConf = NULL; /**< modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /**< modConf ptr to use for the current exec process */

/* tables for interfacing with the v6 config system */
/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
    {"template", eCmdHdlrGetWord, 0},
    {"addlf", eCmdHdlrBinary, 0},
    {"compression.driver", eCmdHdlrGetWord, 0},
    {"compression.zstd.workers", eCmdHdlrPositiveInt, 0},
    {"dircreatemode", eCmdHdlrFileCreateMode, 0},
    {"filecreatemode", eCmdHdlrFileCreateMode, 0},
    {"dirowner", eCmdHdlrUID, 0},
    {"dirownernum", eCmdHdlrInt, 0},
    {"dirgroup", eCmdHdlrGID, 0},
    {"dirgroupnum", eCmdHdlrInt, 0},
    {"fileowner", eCmdHdlrUID, 0},
    {"fileownernum", eCmdHdlrInt, 0},
    {"filegroup", eCmdHdlrGID, 0},
    {"dynafile.donotsuspend", eCmdHdlrBinary, 0},
    {"filegroupnum", eCmdHdlrInt, 0},
};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {{"dynafilecachesize", eCmdHdlrInt, 0}, /* legacy: dynafilecachesize */
                                           {"ziplevel", eCmdHdlrInt, 0}, /* legacy: omfileziplevel */
                                           {"flushinterval", eCmdHdlrInt, 0}, /* legacy: omfileflushinterval */
                                           {"asyncwriting", eCmdHdlrBinary, 0}, /* legacy: omfileasyncwriting */
                                           {"veryrobustzip", eCmdHdlrBinary, 0},
                                           {"flushontxend", eCmdHdlrBinary, 0}, /* legacy: omfileflushontxend */
                                           {"iobuffersize", eCmdHdlrSize, 0}, /* legacy: omfileiobuffersize */
                                           {"dirowner", eCmdHdlrUID, 0}, /* legacy: dirowner */
                                           {"dirownernum", eCmdHdlrInt, 0}, /* legacy: dirownernum */
                                           {"dirgroup", eCmdHdlrGID, 0}, /* legacy: dirgroup */
                                           {"dirgroupnum", eCmdHdlrInt, 0}, /* legacy: dirgroupnum */
                                           {"fileowner", eCmdHdlrUID, 0}, /* legacy: fileowner */
                                           {"fileownernum", eCmdHdlrInt, 0}, /* legacy: fileownernum */
                                           {"filegroup", eCmdHdlrGID, 0}, /* legacy: filegroup */
                                           {"filegroupnum", eCmdHdlrInt, 0}, /* legacy: filegroupnum */
                                           {"dircreatemode", eCmdHdlrFileCreateMode, 0}, /* legacy: dircreatemode */
                                           {"filecreatemode", eCmdHdlrFileCreateMode, 0}, /* legacy: filecreatemode */
                                           {"failonchownfailure", eCmdHdlrBinary, 0}, /* legacy: failonchownfailure */
                                           {"createdirs", eCmdHdlrBinary, 0}, /* legacy: createdirs */
                                           {"sync", eCmdHdlrBinary, 0}, /* legacy: actionfileenablesync */
                                           {"file", eCmdHdlrString, 0}, /* either "file" or ... */
                                           {"dynafile", eCmdHdlrString, 0}, /* "dynafile" MUST be present */
                                           {"sig.provider", eCmdHdlrGetWord, 0},
                                           {"cry.provider", eCmdHdlrGetWord, 0},
                                           {"closetimeout", eCmdHdlrPositiveInt, 0},
                                           {"rotation.sizelimit", eCmdHdlrSize, 0},
                                           {"rotation.sizelimitcommand", eCmdHdlrString, 0},
                                           {"template", eCmdHdlrGetWord, 0},
                                           {"addlf", eCmdHdlrBinary, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};


/**
 * @brief Gets the default template name to be used.
 *
 * This function coordinates the default template name between old-style
 * and new-style configuration parts.
 *
 * @return A pointer to the default template name string.
 */
static uchar *getDfltTpl(void) {
    if (loadModConf != NULL && loadModConf->tplName != NULL)
        return loadModConf->tplName;
    else if (pszFileDfltTplName == NULL)
        return (uchar *)"RSYSLOG_FileFormat";
    else
        return pszFileDfltTplName;
}


BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    pszFileDfltTplName = NULL; /* make sure this can be free'ed! */
    iRet = resetConfigVariables(NULL, NULL); /* params are dummies */
ENDinitConfVars

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    if (pData->bDynamicName) {
        dbgprintf("[dynamic]\n");
    } else { /* regular file */
        dbgprintf("%s%s\n", pData->fname, (pData->pStrm == NULL) ? " (closed)" : "");
    }

    dbgprintf("\ttemplate='%s'\n", pData->fname);
    dbgprintf("\tuse async writer=%d\n", pData->bUseAsyncWriter);
    dbgprintf("\tflush on TX end=%d\n", pData->bFlushOnTXEnd);
    dbgprintf("\tflush interval=%d\n", pData->iFlushInterval);
    dbgprintf("\tappend LF=%d\n", pData->bAddLF);
    dbgprintf("\tfile cache size=%d\n", pData->iDynaFileCacheSize);
    dbgprintf("\tcreate directories: %s\n", pData->bCreateDirs ? "on" : "off");
    dbgprintf("\tvery robust zip: %s\n", pData->bCreateDirs ? "on" : "off");
    dbgprintf("\tfile owner %d, group %d\n", (int)pData->fileUID, (int)pData->fileGID);
    dbgprintf("\tdirectory owner %d, group %d\n", (int)pData->dirUID, (int)pData->dirGID);
    dbgprintf("\tdir create mode 0%3.3o, file create mode 0%3.3o\n", pData->fDirCreateMode, pData->fCreateMode);
    dbgprintf("\tfail if owner/group can not be set: %s\n", pData->bFailOnChown ? "yes" : "no");
ENDdbgPrintInstInfo


/**
 * @brief Sets the default template to be used for output files (legacy method).
 *
 * This is a module-global parameter and needs special handling to coordinate
 * with values set via the v2 config system (rsyslog v6+). This directive is
 * not permitted after the v2 config system has been used to set the parameter.
 *
 * @param pVal Unused pointer, kept for compatibility with command handler signature.
 * @param newVal The new default template name. Ownership is transferred to this function.
 * @return RS_RET_OK on success, RS_RET_ERR if already set by new config system.
 */
static rsRetVal setLegacyDfltTpl(void __attribute__((unused)) * pVal, uchar *newVal) {
    DEFiRet;

    if (loadModConf != NULL && loadModConf->tplName != NULL) {
        free(newVal);
        parser_errmsg(
            "omfile: default template already set via module "
            "global parameter - can no longer be changed");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    free(pszFileDfltTplName);
    pszFileDfltTplName = newVal;
finalize_it:
    RETiRet;
}


/**
 * @brief Sets the dynamic file cache size.
 *
 * Performs limit checking to ensure the new value is valid and logs warnings
 * for unusually large values.
 *
 * @param pVal Unused pointer, kept for compatibility with command handler signature.
 * @param iNewVal The new size for the dynamic file cache.
 * @return RS_RET_OK on success, RS_RET_VAL_OUT_OF_RANGE if the value was invalid
 * (the value will be adjusted to a valid range in this case).
 */
static rsRetVal setDynaFileCacheSize(void __attribute__((unused)) * pVal, int iNewVal) {
    DEFiRet;

    if (iNewVal < 1) {
        errno = 0;
        parser_errmsg("DynaFileCacheSize must be greater 0 (%d given), changed to 1.", iNewVal);
        iRet = RS_RET_VAL_OUT_OF_RANGE;
        iNewVal = 1;
    } else if (iNewVal > 25000) {
        errno = 0;
        parser_warnmsg(
            "DynaFileCacheSize is larger than 25,000 (%d given) - this looks very "
            "large. Is it intended?",
            iNewVal);
    }

    cs.iDynaFileCacheSize = iNewVal;
    DBGPRINTF("DynaFileCacheSize changed to %d.\n", iNewVal);

    RETiRet;
}


/**
 * @brief Parses an output channel name and associates it with the action.
 *
 * This is a helper function to `cfline()`. It parses an output channel name
 * from the configuration line, looks up the channel, and extracts its
 * associated file template and size limit settings, storing them in the
 * provided `instanceData` structure.
 *
 * @param pData Pointer to the instance data structure to populate.
 * @param p Pointer to the current position in the configuration line.
 * @param pOMSR Pointer to the OMOD String Request structure.
 * @param iEntry Index for the OMOD String Request.
 * @param iTplOpts Template options.
 * @return RS_RET_OK on success, or an error code if the output channel
 * is not found or has no valid file name template.
 */
static rsRetVal cflineParseOutchannel(
    instanceData *pData, uchar *p, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts) {
    DEFiRet;
    size_t i;
    struct outchannel *pOch;
    char szBuf[128]; /* should be more than sufficient */

    ++p; /* skip '$' */
    i = 0;
    /* get outchannel name */
    while (*p && *p != ';' && *p != ' ' && i < (sizeof(szBuf) - 1)) {
        szBuf[i++] = *p++;
    }
    szBuf[i] = '\0';

    /* got the name, now look up the channel... */
    pOch = ochFind(szBuf, i);

    if (pOch == NULL) {
        parser_errmsg("outchannel '%s' not found - ignoring action line", szBuf);
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }

    /* check if there is a file name in the outchannel... */
    if (pOch->pszFileTemplate == NULL) {
        parser_errmsg("outchannel '%s' has no file name template - ignoring action line", szBuf);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* OK, we finally got a correct template. So let's use it... */
    pData->fname = ustrdup(pOch->pszFileTemplate);
    pData->iSizeLimit = pOch->uSizeLimit;
    /* WARNING: It is dangerous "just" to pass the pointer. As we
     * never rebuild the output channel description, this is acceptable here.
     */
    pData->pszSizeLimitCmd = pOch->cmdOnSizeLimit;

    iRet = cflineParseTemplateName(&p, pOMSR, iEntry, iTplOpts, getDfltTpl());

finalize_it:
    RETiRet;
}


/**
 * @brief Deletes an entry from the dynamic file name cache.
 *
 * This function closes the associated file stream, frees memory for the
 * file name, and optionally frees the cache entry structure itself.
 *
 * @param pData Pointer to the instance data containing the dynamic file cache.
 * @param iEntry The index of the entry to be deleted in the cache array.
 * @param bFreeEntry If 1, the cache entry structure itself is free()ed;
 * if 0, only its contents are freed (e.g., if it's being
 * reused for a new entry).
 * @return RS_RET_OK on success.
 */
static rsRetVal dynaFileDelCacheEntry(instanceData *__restrict__ const pData, const int iEntry, const int bFreeEntry) {
    dynaFileCacheEntry **pCache = pData->dynCache;
    DEFiRet;
    assert(pCache != NULL);

    if (pCache[iEntry] == NULL) FINALIZE;

    DBGPRINTF("Removing entry %d for file '%s' from dynaCache.\n", iEntry,
              pCache[iEntry]->pName == NULL ? UCHAR_CONSTANT("[OPEN FAILED]") : pCache[iEntry]->pName);

    if (pCache[iEntry]->pName != NULL) {
        free(pCache[iEntry]->pName);
        pCache[iEntry]->pName = NULL;
    }

    if (pCache[iEntry]->pStrm != NULL) {
        if (iEntry == pData->iCurrElt) {
            pData->iCurrElt = -1;
            pData->pStrm = NULL;
        }
        strm.Destruct(&pCache[iEntry]->pStrm);
        if (pData->useSigprov) {
            pData->sigprov.OnFileClose(pCache[iEntry]->sigprovFileData);
            pCache[iEntry]->sigprovFileData = NULL;
        }
    }

    if (bFreeEntry) {
        free(pCache[iEntry]);
        pCache[iEntry] = NULL;
    }

finalize_it:
    RETiRet;
}


/**
 * @brief Frees all dynamic file name cache entries and closes relevant files.
 *
 * This function is typically called during module shutdown or HUP processing
 * to release all resources held by the dynamic file cache.
 *
 * @param pData Pointer to the instance data containing the dynamic file cache.
 */
static void dynaFileFreeCacheEntries(instanceData *__restrict__ const pData) {
    register int i;
    assert(pData != NULL);

    for (i = 0; i < pData->iCurrCacheSize; ++i) {
        dynaFileDelCacheEntry(pData, i, 1);
    }
    /* invalidate current element */
    pData->iCurrElt = -1;
    pData->pStrm = NULL;
}


/**
 * @brief Frees the dynamic file name cache structure.
 *
 * This function first frees all entries within the cache and then
 * deallocates the cache array itself.
 *
 * @param pData Pointer to the instance data containing the dynamic file cache.
 */
static void dynaFileFreeCache(instanceData *__restrict__ const pData) {
    assert(pData != NULL);

    dynaFileFreeCacheEntries(pData);
    if (pData->dynCache != NULL) free(pData->dynCache);
}


/**
 * @brief Closes the currently open file stream for a given instance.
 *
 * If a signature provider is in use, its `OnFileClose` method is also called.
 *
 * @param pData Pointer to the instance data whose file stream is to be closed.
 * @return RS_RET_OK on success.
 */
static rsRetVal closeFile(instanceData *__restrict__ const pData) {
    DEFiRet;
    if (pData->useSigprov) {
        pData->sigprov.OnFileClose(pData->sigprovFileData);
        pData->sigprovFileData = NULL;
    }
    strm.Destruct(&pData->pStrm);
    RETiRet;
}


/**
 * @brief Prepares the signature provider for processing a file.
 *
 * This function calls the `OnFileOpen` method of the configured signature
 * provider, passing the file name and obtaining an opaque data pointer for
 * file-specific signature operations.
 *
 * @param pData Pointer to the instance data containing the signature provider.
 * @param fn The name of the file to be processed.
 * @return RS_RET_OK on success.
 */
static rsRetVal sigprovPrepare(instanceData *__restrict__ const pData, uchar *__restrict__ const fn) {
    DEFiRet;
    pData->sigprov.OnFileOpen(pData->sigprovData, fn, &pData->sigprovFileData);
    RETiRet;
}

/**
 * @brief Prepares file access for a given file name.
 *
 * This function handles the creation of parent directories (if configured),
 * creates the file itself (if it doesn't exist), sets its ownership and
 * permissions, and then constructs and finalizes the rsyslog stream object
 * for writing to the file. It also initializes signature and crypto providers
 * if enabled.
 *
 * @param pData Pointer to the instance data for the file output action.
 * @param newFileName The name of the file to prepare access for.
 * @return RS_RET_OK on success, or an error code if file creation,
 * directory creation, or stream construction fails.
 */
static rsRetVal prepareFile(instanceData *__restrict__ const pData, const uchar *__restrict__ const newFileName) {
    int fd;
    char errStr[1024]; /* buffer for strerr() */
    DEFiRet;

    pData->pStrm = NULL;
    if (access((char *)newFileName, F_OK) != 0) {
        /* file does not exist, create it (and eventually parent directories */
        if (pData->bCreateDirs) {
            /* We first need to create parent dirs if they are missing.
             * We do not report any errors here ourselfs but let the code
             * fall through to error handler below.
             */
            if (makeFileParentDirs(newFileName, ustrlen(newFileName), pData->fDirCreateMode, pData->dirUID,
                                   pData->dirGID, pData->bFailOnChown) != 0) {
                rs_strerror_r(errno, errStr, sizeof(errStr));
                parser_errmsg(
                    "omfile: creating parent "
                    "directories for file  '%s' failed: %s",
                    newFileName, errStr);
                ABORT_FINALIZE(RS_RET_ERR); /* we give up */
            }
        }
        /* no matter if we needed to create directories or not, we now try to create
         * the file. -- rgerhards, 2008-12-18 (based on patch from William Tisater)
         */
        fd = open((char *)newFileName, O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY | O_CLOEXEC, pData->fCreateMode);
        if (fd != -1) {
            /* check and set uid/gid */
            if (pData->fileUID != (uid_t)-1 || pData->fileGID != (gid_t)-1) {
                /* we need to set owner/group */
                if (fchown(fd, pData->fileUID, pData->fileGID) != 0) {
                    rs_strerror_r(errno, errStr, sizeof(errStr));
                    parser_errmsg("omfile: chown for file '%s' failed: %s", newFileName, errStr);
                    if (pData->bFailOnChown) {
                        close(fd);
                        ABORT_FINALIZE(RS_RET_ERR); /* we give up */
                    }
                    /* we will silently ignore the chown() failure
                     * if configured to do so.
                     */
                }
            }
            close(fd); /* close again, as we need a stream further on */
        }
    }

    /* the copies below are clumpsy, but there is no way around given the
     * anomalies in dirname() and basename() [they MODIFY the provided buffer...]
     */
    uchar szNameBuf[MAXFNAME + 1];
    uchar szDirName[MAXFNAME + 1];
    uchar szBaseName[MAXFNAME + 1];
    ustrncpy(szNameBuf, newFileName, MAXFNAME);
    szNameBuf[MAXFNAME] = '\0';
    ustrncpy(szDirName, (uchar *)dirname((char *)szNameBuf), MAXFNAME);
    szDirName[MAXFNAME] = '\0';
    ustrncpy(szNameBuf, newFileName, MAXFNAME);
    szNameBuf[MAXFNAME] = '\0';
    ustrncpy(szBaseName, (uchar *)basename((char *)szNameBuf), MAXFNAME);
    szBaseName[MAXFNAME] = '\0';

    CHKiRet(strm.Construct(&pData->pStrm));
    CHKiRet(strm.SetFName(pData->pStrm, szBaseName, ustrlen(szBaseName)));
    CHKiRet(strm.SetDir(pData->pStrm, szDirName, ustrlen(szDirName)));
    CHKiRet(strm.SetiZipLevel(pData->pStrm, pData->iZipLevel));
    CHKiRet(strm.SetbVeryReliableZip(pData->pStrm, pData->bVeryRobustZip));
    CHKiRet(strm.SetsIOBufSize(pData->pStrm, (size_t)pData->iIOBufSize));
    CHKiRet(strm.SettOperationsMode(pData->pStrm, STREAMMODE_WRITE_APPEND));
    CHKiRet(strm.SettOpenMode(pData->pStrm, cs.fCreateMode));
    CHKiRet(strm.SetcompressionDriver(pData->pStrm, runModConf->compressionDriver));
    CHKiRet(strm.SetCompressionWorkers(pData->pStrm, runModConf->compressionDriver_workers));
    CHKiRet(strm.SetbSync(pData->pStrm, pData->bSyncFile));
    CHKiRet(strm.SetsType(pData->pStrm, STREAMTYPE_FILE_SINGLE));
    CHKiRet(strm.SetiSizeLimit(pData->pStrm, pData->iSizeLimit));
    if (pData->useCryprov) {
        CHKiRet(strm.Setcryprov(pData->pStrm, &pData->cryprov));
        CHKiRet(strm.SetcryprovData(pData->pStrm, pData->cryprovData));
    }
    /* set the flush interval only if we actually use it - otherwise it will activate
     * async processing, which is a real performance waste if we do not do buffered
     * writes! -- rgerhards, 2009-07-06
     */
    if (pData->bUseAsyncWriter) CHKiRet(strm.SetiFlushInterval(pData->pStrm, pData->iFlushInterval));
    if (pData->pszSizeLimitCmd != NULL) CHKiRet(strm.SetpszSizeLimitCmd(pData->pStrm, ustrdup(pData->pszSizeLimitCmd)));
    CHKiRet(strm.ConstructFinalize(pData->pStrm));

    if (pData->useSigprov) sigprovPrepare(pData, szNameBuf);

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pData->pStrm != NULL) {
            closeFile(pData);
        }
    }
    RETiRet;
}


/**
 * @brief Handles dynamic file names, ensuring the correct file is open for writing.
 *
 * This function checks if the requested dynamic file name is already present
 * in the cache. If so, it reuses the existing file handle. If not, it attempts
 * to open the new file, potentially evicting an older entry from the LRU cache
 * if the cache is full.
 *
 * @param pData Pointer to the instance data for the file output action.
 * @param newFileName The new dynamic file name to prepare.
 * @return RS_RET_OK on success, or an error code if the file cannot be opened
 * or memory allocation fails.
 */
static rsRetVal ATTR_NONNULL()
    prepareDynFile(instanceData *__restrict__ const pData, const uchar *__restrict__ const newFileName) {
    uint64 ctOldest; /* "timestamp" of oldest element */
    int iOldest;
    int i;
    int iFirstFree;
    rsRetVal localRet;
    dynaFileCacheEntry **pCache;
    DEFiRet;

    assert(pData != NULL);
    assert(newFileName != NULL);

    pCache = pData->dynCache;

    /* first check, if we still have the current file */
    if ((pData->iCurrElt != -1) && !ustrcmp(newFileName, pCache[pData->iCurrElt]->pName)) {
        /* great, we are all set */
        pCache[pData->iCurrElt]->clkTickAccessed = getClockFileAccess();
        STATSCOUNTER_INC(pData->ctrLevel0, pData->mutCtrLevel0);
        /* LRU needs only a strictly monotonically increasing counter, so such a one could do */
        FINALIZE;
    }

    /* ok, no luck - current file cannot be re-used */

    /* if we need to flush (at least) on TXEnd, we need to flush now - because
     * we do not know if we will otherwise come back to this file to flush it
     * at end of TX. see https://github.com/rsyslog/rsyslog/issues/2502
     */
    if (((runModConf->pConf->globals.glblDevOptions & DEV_OPTION_8_1905_HANG_TEST) == 0) && pData->bFlushOnTXEnd &&
        pData->pStrm != NULL) {
        CHKiRet(strm.Flush(pData->pStrm));
    }

    /* Now let's search the table if we find a matching spot.
     * While doing so, we also prepare for creation of a new one.
     */
    pData->iCurrElt = -1; /* invalid current element pointer */
    iFirstFree = -1; /* not yet found */
    iOldest = 0; /* we assume the first element to be the oldest - that will change as we loop */
    ctOldest = getClockFileAccess(); /* there must always be an older one */
    for (i = 0; i < pData->iCurrCacheSize; ++i) {
        if (pCache[i] == NULL || pCache[i]->pName == NULL) {
            if (iFirstFree == -1) iFirstFree = i;
        } else { /* got an element, let's see if it matches */
            if (!ustrcmp(newFileName, pCache[i]->pName)) {
                /* we found our element! */
                pData->pStrm = pCache[i]->pStrm;
                if (pData->useSigprov) pData->sigprovFileData = pCache[i]->sigprovFileData;
                pData->iCurrElt = i;
                pCache[i]->clkTickAccessed = getClockFileAccess(); /* update "timestamp" for LRU */
                FINALIZE;
            }
            /* did not find it - so lets keep track of the counters for LRU */
            if (pCache[i]->clkTickAccessed < ctOldest) {
                ctOldest = pCache[i]->clkTickAccessed;
                iOldest = i;
            }
        }
    }

    /* we have not found an entry */
    STATSCOUNTER_INC(pData->ctrMiss, pData->mutCtrMiss);

    /* similarly, we need to set the current pStrm to NULL, because otherwise, if prepareFile() fails,
     * we may end up using an old stream. This bug depends on how exactly prepareFile fails,
     * but it could be triggered in the common case of a failed open() system call.
     * rgerhards, 2010-03-22
     */
    pData->pStrm = NULL, pData->sigprovFileData = NULL;

    if (iFirstFree == -1 && (pData->iCurrCacheSize < pData->iDynaFileCacheSize)) {
        /* there is space left, so set it to that index */
        iFirstFree = pData->iCurrCacheSize++;
        STATSCOUNTER_SETMAX_NOMUT(pData->ctrMax, (unsigned)pData->iCurrCacheSize);
    }

    /* Note that the following code sequence does not work with the cache entry itself,
     * but rather with pData->pStrm, the (sole) stream pointer in the non-dynafile case.
     * The cache array is only updated after the open was successful. -- rgerhards, 2010-03-21
     */
    if (iFirstFree == -1) {
        dynaFileDelCacheEntry(pData, iOldest, 0);
        STATSCOUNTER_INC(pData->ctrEvict, pData->mutCtrEvict);
        iFirstFree = iOldest; /* this one *is* now free ;) */
    } else {
        /* we need to allocate memory for the cache structure */
        CHKmalloc(pCache[iFirstFree] = (dynaFileCacheEntry *)calloc(1, sizeof(dynaFileCacheEntry)));
    }

    /* Ok, we finally can open the file */
    localRet = prepareFile(pData, newFileName); /* ignore exact error, we check fd below */

    /* check if we had an error */
    if (localRet != RS_RET_OK) {
        /* We do no longer care about internal messages. The errmsg rate limiter
         * will take care of too-frequent error messages.
         */
        parser_errmsg(
            "Could not open dynamic file '%s' [state %d] - discarding "
            "message",
            newFileName, localRet);
        ABORT_FINALIZE(localRet);
    }

    if ((pCache[iFirstFree]->pName = ustrdup(newFileName)) == NULL) {
        closeFile(pData); /* need to free failed entry! */
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    pCache[iFirstFree]->pStrm = pData->pStrm;
    if (pData->useSigprov) pCache[iFirstFree]->sigprovFileData = pData->sigprovFileData;
    pCache[iFirstFree]->clkTickAccessed = getClockFileAccess();
    pData->iCurrElt = iFirstFree;
    DBGPRINTF("Added new entry %d for file cache, file '%s'.\n", iFirstFree, newFileName);

finalize_it:
    if (iRet == RS_RET_OK) pCache[pData->iCurrElt]->nInactive = 0;
    RETiRet;
}


/**
 * @brief Performs the actual buffered write operation to the file stream.
 *
 * This function writes the provided buffer to the stream associated with
 * the instance data. It also calls the signature provider's `OnRecordWrite`
 * method if a signature provider is in use.
 *
 * @param pData Pointer to the instance data containing the stream and provider info.
 * @param pszBuf Pointer to the buffer containing the data to write.
 * @param lenBuf The length of the data in the buffer.
 * @return RS_RET_OK on success, or an error code from the stream write operation.
 */
static rsRetVal doWrite(instanceData *__restrict__ const pData, uchar *__restrict__ const pszBuf, const int lenBuf) {
    DEFiRet;
    assert(pData != NULL);
    assert(pszBuf != NULL);

    const int needsLF = pData->bAddLF && (lenBuf == 0 || pszBuf[lenBuf - 1] != '\n');
    uchar addlfBuf[ADDLF_SIGBUF_STACKLEN];
    uchar *sigBuf = NULL;
    size_t sigLen = 0;
    int freeSigBuf = 0;

    DBGPRINTF("omfile: write to stream, pData->pStrm %p, lenBuf %d, needsLF %d, strt data %.128s\n", pData->pStrm,
              lenBuf, needsLF, pszBuf);
    if (pData->pStrm != NULL) {
        if (lenBuf > 0) {
            CHKiRet(strm.Write(pData->pStrm, pszBuf, lenBuf));
        }
        if (needsLF) {
            if (pData->useSigprov) {
                sigLen = (size_t)lenBuf + 1;
                if (sigLen <= sizeof(addlfBuf)) {
                    sigBuf = addlfBuf;
                } else {
                    sigBuf = (uchar *)malloc(sigLen);
                    if (sigBuf == NULL) {
                        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                    }
                    freeSigBuf = 1;
                }
                if (lenBuf > 0) {
                    memcpy(sigBuf, pszBuf, (size_t)lenBuf);
                }
                sigBuf[lenBuf] = '\n';
            }
            CHKiRet(strm.WriteChar(pData->pStrm, '\n'));
        }
        if (pData->useSigprov) {
            if (needsLF) {
                CHKiRet(pData->sigprov.OnRecordWrite(pData->sigprovFileData, sigBuf, (rs_size_t)sigLen));
            } else {
                CHKiRet(pData->sigprov.OnRecordWrite(pData->sigprovFileData, pszBuf, lenBuf));
            }
        }
    }

finalize_it:
    if (freeSigBuf) {
        free(sigBuf);
    }
    RETiRet;
}


/**
 * @brief Writes a message to the configured file.
 *
 * This function determines whether the output is to a static file or a
 * dynamic file. If dynamic, it calls `prepareDynFile` to ensure the correct
 * file is open. It then calls `doWrite` to perform the actual write operation.
 *
 * @param pData Pointer to the instance data for the file output action.
 * @param pParam Pointer to the action worker instance parameters.
 * @param iMsg The index of the message within the batch to process.
 * @return RS_RET_OK on success, or an error code if file preparation
 * or writing fails.
 */
static rsRetVal writeFile(instanceData *__restrict__ const pData,
                          const actWrkrIParams_t *__restrict__ const pParam,
                          const int iMsg) {
    DEFiRet;

    STATSCOUNTER_INC(pData->ctrRequests, pData->mutCtrRequests);
    /* first check if we have a dynamic file name and, if so,
     * check if it still is ok or a new file needs to be created
     */
    if (pData->bDynamicName) {
        DBGPRINTF("omfile: file to log to: %s\n", actParam(pParam, pData->iNumTpls, iMsg, 1).param);
        CHKiRet(prepareDynFile(pData, actParam(pParam, pData->iNumTpls, iMsg, 1).param));
    } else { /* "regular", non-dynafile */
        if (pData->pStrm == NULL) {
            CHKiRet(prepareFile(pData, pData->fname));
            if (pData->pStrm == NULL) {
                parser_errmsg("Could not open output file '%s'", pData->fname);
            }
        }
        pData->nInactive = 0;
    }

    iRet = doWrite(pData, actParam(pParam, pData->iNumTpls, iMsg, 0).param,
                   actParam(pParam, pData->iNumTpls, iMsg, 0).lenStr);

finalize_it:
    RETiRet;
}


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    pModConf->tplName = NULL;
    pModConf->fCreateMode = 0644;
    pModConf->fDirCreateMode = 0700;
    pModConf->fileUID = -1;
    pModConf->dirUID = -1;
    pModConf->fileGID = -1;
    pModConf->dirGID = -1;
    pModConf->bDynafileDoNotSuspend = 1;
    pModConf->addLF = 0;
ENDbeginCnfLoad

BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    int i;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        parser_errmsg(
            "error processing module "
            "config parameters [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("module (global) param blk for omfile:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) {
            continue;
        }

        if (!strcmp(modpblk.descr[i].name, "template")) {
            loadModConf->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (pszFileDfltTplName != NULL) {
                parser_errmsg(
                    "omfile: warning: default template was already "
                    "set via legacy directive - may lead to inconsistent "
                    "results.");
            }
        } else if (!strcmp(modpblk.descr[i].name, "addlf")) {
            loadModConf->addLF = pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "compression.driver")) {
            if (!es_strcasebufcmp(pvals[i].val.d.estr, (const unsigned char *)"zlib", 4)) {
                loadModConf->compressionDriver = STRM_COMPRESS_ZIP;
            } else if (!es_strcasebufcmp(pvals[i].val.d.estr, (const unsigned char *)"zstd", 4)) {
                loadModConf->compressionDriver = STRM_COMPRESS_ZSTD;
            } else {
                parser_errmsg(
                    "omfile: error: invalid compression.driver driver "
                    "name - noch applying setting. Valid drivers: 'zlib' and "
                    "'zstd'.");
            }
        } else if (!strcmp(modpblk.descr[i].name, "compression.zstd.workers")) {
            loadModConf->compressionDriver_workers = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "dircreatemode")) {
            loadModConf->fDirCreateMode = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "filecreatemode")) {
            loadModConf->fCreateMode = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "dirowner")) {
            loadModConf->dirUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "dirownernum")) {
            loadModConf->dirUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "dirgroup")) {
            loadModConf->dirGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "dirgroupnum")) {
            loadModConf->dirGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "fileowner")) {
            loadModConf->fileUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "fileownernum")) {
            loadModConf->fileUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "filegroup")) {
            loadModConf->fileGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "filegroupnum")) {
            loadModConf->fileGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "dynafile.donotsuspend")) {
            loadModConf->bDynafileDoNotSuspend = (int)pvals[i].val.d.n;
        } else {
            dbgprintf(
                "omfile: program error, non-handled "
                "param '%s' in beginCnfLoad\n",
                modpblk.descr[i].name);
        }
    }
finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

/**
 * @brief Checks the dynamic file cache for entries exceeding their close timeout.
 *
 * This function iterates through the dynamic file cache and closes/deletes
 * entries that have been inactive for longer than their configured
 * `iCloseTimeout`. It is typically called by the janitor.
 *
 * @param pData Pointer to the instance data containing the dynamic file cache.
 */
static void janitorChkDynaFiles(instanceData *__restrict__ const pData) {
    int i;
    dynaFileCacheEntry **pCache = pData->dynCache;

    for (i = 0; i < pData->iCurrCacheSize; ++i) {
        if (pCache[i] == NULL) continue;
        DBGPRINTF("omfile janitor: checking dynafile %d:%s, inactive since %d\n", i,
                  pCache[i]->pName == NULL ? UCHAR_CONSTANT("[OPEN FAILED]") : pCache[i]->pName,
                  (int)pCache[i]->nInactive);
        if (pCache[i]->nInactive >= pData->iCloseTimeout) {
            STATSCOUNTER_INC(pData->ctrCloseTimeouts, pData->mutCtrCloseTimeouts);
            dynaFileDelCacheEntry(pData, i, 1);
            if (pData->iCurrElt == i) pData->iCurrElt = -1; /* no longer available! */
        } else {
            pCache[i]->nInactive += runModConf->pConf->globals.janitorInterval;
        }
    }
}

/**
 * @brief Callback function for the janitor.
 *
 * This function is called periodically by the rsyslog janitor to clean out
 * inactive files (both static and dynamic) based on their configured
 * `iCloseTimeout`. It acquires a mutex to protect access to the file data.
 *
 * @param pUsr A pointer to the `instanceData` structure for the file action.
 */
static void janitorCB(void *pUsr) {
    instanceData *__restrict__ const pData = (instanceData *)pUsr;
    pthread_mutex_lock(&pData->mutWrite);
    if (pData->bDynamicName) {
        janitorChkDynaFiles(pData);
    } else {
        if (pData->pStrm != NULL) {
            DBGPRINTF("omfile janitor: checking file %s, inactive since %d\n", pData->fname, pData->nInactive);
            if (pData->nInactive >= pData->iCloseTimeout) {
                STATSCOUNTER_INC(pData->ctrCloseTimeouts, pData->mutCtrCloseTimeouts);
                closeFile(pData);
            } else {
                pData->nInactive += runModConf->pConf->globals.janitorInterval;
            }
        }
    }
    pthread_mutex_unlock(&pData->mutWrite);
}


BEGINendCnfLoad
    CODESTARTendCnfLoad;
    loadModConf = NULL; /* done loading */
    /* free legacy config vars */
    free(pszFileDfltTplName);
    pszFileDfltTplName = NULL;
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
    free(pModConf->tplName);
ENDfreeCnf


BEGINcreateInstance
    CODESTARTcreateInstance;
    pData->pStrm = NULL;
    pData->bAddLF = 0;
    pthread_mutex_init(&pData->mutWrite, NULL);
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance


BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->tplName);
    free(pData->fname);
    if (pData->iCloseTimeout > 0) janitorDelEtry(pData->janitorID);
    if (pData->bDynamicName) {
        dynaFileFreeCache(pData);
    } else if (pData->pStrm != NULL)
        closeFile(pData);
    if (pData->stats != NULL) statsobj.Destruct(&(pData->stats));
    if (pData->useSigprov) {
        pData->sigprov.Destruct(&pData->sigprovData);
        obj.ReleaseObj(__FILE__, pData->sigprovNameFull + 2, pData->sigprovNameFull, (void *)&pData->sigprov);
        free(pData->sigprovName);
        free(pData->sigprovNameFull);
    }
    if (pData->useCryprov) {
        pData->cryprov.Destruct(&pData->cryprovData);
        obj.ReleaseObj(__FILE__, pData->cryprovNameFull + 2, pData->cryprovNameFull, (void *)&pData->cryprov);
        free(pData->cryprovName);
        free(pData->cryprovNameFull);
    }
    pthread_mutex_destroy(&pData->mutWrite);
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume

BEGINbeginTransaction
    CODESTARTbeginTransaction;
    /* we have nothing to do to begin a transaction */
ENDbeginTransaction


BEGINcommitTransaction
    instanceData *__restrict__ const pData = pWrkrData->pData;
    unsigned i;
    CODESTARTcommitTransaction;

    if (pData->isDevNull) {
        goto terminate;
    }

    pthread_mutex_lock(&pData->mutWrite);

    for (i = 0; i < nParams; ++i) {
        writeFile(pData, pParams, i);
    }
    /* Note: pStrm may be NULL if there was an error opening the stream */
    /* if bFlushOnTXEnd is set, we need to flush on transaction end - in
     * any case. It is not relevant if this is using background writes
     * (which then become pretty slow) or not. And, similarly, no flush
     * happens when it is not set. Please see
     * https://github.com/rsyslog/rsyslog/issues/1297
     * for a discussion of why we actually need this.
     * rgerhards, 2017-01-13
     */
    if (pData->bFlushOnTXEnd && pData->pStrm != NULL) {
        CHKiRet(strm.Flush(pData->pStrm));
    }

finalize_it:
    pthread_mutex_unlock(&pData->mutWrite);
    if (iRet == RS_RET_FILE_OPEN_ERROR || iRet == RS_RET_FILE_NOT_FOUND) {
        iRet = (pData->bDynamicName && runModConf->bDynafileDoNotSuspend) ? RS_RET_OK : RS_RET_SUSPENDED;
    }

terminate:
ENDcommitTransaction

/**
 * @brief Sets default parameter values for a new omfile action instance.
 *
 * This function initializes various fields of the `instanceData` structure
 * with their default values, some of which are derived from module-global
 * configuration settings.
 *
 * @param pData Pointer to the `instanceData` structure to be initialized.
 */
static void setInstParamDefaults(instanceData *__restrict__ const pData) {
    pData->fname = NULL;
    pData->tplName = NULL;
    pData->fileUID = loadModConf->fileUID;
    pData->fileGID = loadModConf->fileGID;
    pData->dirUID = loadModConf->dirUID;
    pData->dirGID = loadModConf->dirGID;
    pData->bFailOnChown = 1;
    pData->iDynaFileCacheSize = 10;
    pData->fCreateMode = loadModConf->fCreateMode;
    pData->fDirCreateMode = loadModConf->fDirCreateMode;
    pData->bCreateDirs = 1;
    pData->bSyncFile = 0;
    pData->iZipLevel = 0;
    pData->bVeryRobustZip = 0;
    pData->bFlushOnTXEnd = FLUSHONTX_DFLT;
    pData->iIOBufSize = IOBUF_DFLT_SIZE;
    pData->iFlushInterval = FLUSH_INTRVL_DFLT;
    pData->bUseAsyncWriter = USE_ASYNCWRITER_DFLT;
    pData->bAddLF = loadModConf->addLF;
    pData->sigprovName = NULL;
    pData->cryprovName = NULL;
    pData->useSigprov = 0;
    pData->useCryprov = 0;
    pData->iCloseTimeout = -1;
    pData->iSizeLimit = 0;
    pData->isDevNull = 0;
    pData->pszSizeLimitCmd = NULL;
}

/**
 * @brief Sets up statistics counters for a new omfile instance.
 *
 * This function initializes and registers statistics counters for dynamic
 * file actions, including requests, cache hits/misses, evictions, and
 * close timeouts.
 *
 * @param pData Pointer to the `instanceData` structure for which to set up stats.
 * @return RS_RET_OK on success, or an error code if statsobj construction fails.
 */
static rsRetVal setupInstStatsCtrs(instanceData *__restrict__ const pData) {
    uchar ctrName[512];
    DEFiRet;

    if (!pData->bDynamicName) {
        FINALIZE;
    }

    /* support statistics gathering */
    snprintf((char *)ctrName, sizeof(ctrName), "dynafile cache %s", pData->fname);
    ctrName[sizeof(ctrName) - 1] = '\0'; /* be on the save side */
    CHKiRet(statsobj.Construct(&(pData->stats)));
    CHKiRet(statsobj.SetName(pData->stats, ctrName));
    CHKiRet(statsobj.SetOrigin(pData->stats, (uchar *)"omfile"));
    STATSCOUNTER_INIT(pData->ctrRequests, pData->mutCtrRequests);
    CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("requests"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pData->ctrRequests)));
    STATSCOUNTER_INIT(pData->ctrLevel0, pData->mutCtrLevel0);
    CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("level0"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pData->ctrLevel0)));
    STATSCOUNTER_INIT(pData->ctrMiss, pData->mutCtrMiss);
    CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("missed"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pData->ctrMiss)));
    STATSCOUNTER_INIT(pData->ctrEvict, pData->mutCtrEvict);
    CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("evicted"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pData->ctrEvict)));
    STATSCOUNTER_INIT(pData->ctrMax, pData->mutCtrMax);
    CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("maxused"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pData->ctrMax)));
    STATSCOUNTER_INIT(pData->ctrCloseTimeouts, pData->mutCtrCloseTimeouts);
    CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("closetimeouts"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pData->ctrCloseTimeouts)));
    CHKiRet(statsobj.ConstructFinalize(pData->stats));

finalize_it:
    RETiRet;
}

/**
 * @brief Initializes the signature provider for an omfile instance.
 *
 * This function loads the specified signature provider module, constructs
 * its data instance, and sets its configuration parameters. If any step
 * fails, signatures are disabled for this instance.
 *
 * @param pData Pointer to the `instanceData` structure.
 * @param lst List of configuration parameters for the signature provider.
 */
static void initSigprov(instanceData *__restrict__ const pData, struct nvlst *lst) {
    uchar szDrvrName[1024];

    if (snprintf((char *)szDrvrName, sizeof(szDrvrName), "lmsig_%s", pData->sigprovName) == sizeof(szDrvrName)) {
        parser_errmsg(
            "omfile: signature provider "
            "name is too long: '%s' - signatures disabled",
            pData->sigprovName);
        goto done;
    }
    pData->sigprovNameFull = ustrdup(szDrvrName);

    pData->sigprov.ifVersion = sigprovCURR_IF_VERSION;
    /* The pDrvrName+2 below is a hack to obtain the object name. It
     * safes us to have yet another variable with the name without "lm" in
     * front of it. If we change the module load interface, we may re-think
     * about this hack, but for the time being it is efficient and clean enough.
     */
    if (obj.UseObj(__FILE__, szDrvrName, szDrvrName, (void *)&pData->sigprov) != RS_RET_OK) {
        parser_errmsg(
            "omfile: could not load "
            "signature provider '%s' - signatures disabled",
            szDrvrName);
        goto done;
    }

    if (pData->sigprov.Construct(&pData->sigprovData) != RS_RET_OK) {
        parser_errmsg(
            "omfile: error constructing "
            "signature provider %s dataset - signatures disabled",
            szDrvrName);
        goto done;
    }
    pData->sigprov.SetCnfParam(pData->sigprovData, lst);

    dbgprintf("loaded signature provider %s, data instance at %p\n", szDrvrName, pData->sigprovData);
    pData->useSigprov = 1;
done:
    return;
}

/**
 * @brief Initializes the crypto provider for an omfile instance.
 *
 * This function loads the specified crypto provider module, constructs
 * its data instance, and sets its configuration parameters. If any step
 * fails, encryption is disabled for this instance.
 *
 * @param pData Pointer to the `instanceData` structure.
 * @param lst List of configuration parameters for the crypto provider.
 * @return RS_RET_OK on success, or an error code if loading or construction fails.
 */
static rsRetVal initCryprov(instanceData *__restrict__ const pData, struct nvlst *lst) {
    uchar szDrvrName[1024];
    DEFiRet;

    if (snprintf((char *)szDrvrName, sizeof(szDrvrName), "lmcry_%s", pData->cryprovName) == sizeof(szDrvrName)) {
        parser_errmsg(
            "omfile: crypto provider "
            "name is too long: '%s' - encryption disabled",
            pData->cryprovName);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pData->cryprovNameFull = ustrdup(szDrvrName);

    pData->cryprov.ifVersion = cryprovCURR_IF_VERSION;
    /* The pDrvrName+2 below is a hack to obtain the object name. It
     * safes us to have yet another variable with the name without "lm" in
     * front of it. If we change the module load interface, we may re-think
     * about this hack, but for the time being it is efficient and clean enough.
     */
    if (obj.UseObj(__FILE__, szDrvrName, szDrvrName, (void *)&pData->cryprov) != RS_RET_OK) {
        parser_errmsg(
            "omfile: could not load "
            "crypto provider '%s' - encryption disabled",
            szDrvrName);
        ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
    }

    if (pData->cryprov.Construct(&pData->cryprovData) != RS_RET_OK) {
        parser_errmsg(
            "omfile: error constructing "
            "crypto provider %s dataset - encryption disabled",
            szDrvrName);
        ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
    }
    CHKiRet(pData->cryprov.SetCnfParam(pData->cryprovData, lst, CRYPROV_PARAMTYPE_REGULAR));

    dbgprintf("loaded crypto provider %s, data instance at %p\n", szDrvrName, pData->cryprovData);
    pData->useCryprov = 1;
finalize_it:
    RETiRet;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    uchar *tplToUse;
    int i;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (omfile)\n");

    pvals = nvlstGetParams(lst, &actpblk, NULL);
    if (pvals == NULL) {
        parser_errmsg(
            "omfile: either the \"file\" or "
            "\"dynafile\" parameter must be given");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("action param blk in omfile:\n");
        cnfparamsPrint(&actpblk, pvals);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "dynafilecachesize")) {
            pData->iDynaFileCacheSize = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "ziplevel")) {
            pData->iZipLevel = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "flushinterval")) {
            pData->iFlushInterval = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "veryrobustzip")) {
            pData->bVeryRobustZip = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "asyncwriting")) {
            pData->bUseAsyncWriter = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "flushontxend")) {
            pData->bFlushOnTXEnd = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "iobuffersize")) {
            pData->iIOBufSize = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "dirowner")) {
            pData->dirUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "dirownernum")) {
            pData->dirUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "dirgroup")) {
            pData->dirGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "dirgroupnum")) {
            pData->dirGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "fileowner")) {
            pData->fileUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "fileownernum")) {
            pData->fileUID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "filegroup")) {
            pData->fileGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "filegroupnum")) {
            pData->fileGID = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "dircreatemode")) {
            pData->fDirCreateMode = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "filecreatemode")) {
            pData->fCreateMode = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "failonchownfailure")) {
            pData->bFailOnChown = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "sync")) {
            pData->bSyncFile = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "createdirs")) {
            pData->bCreateDirs = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "addlf")) {
            pData->bAddLF = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "file")) {
            pData->fname = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            CODE_STD_STRING_REQUESTnewActInst(1);
            pData->bDynamicName = 0;
        } else if (!strcmp(actpblk.descr[i].name, "dynafile")) {
            if (pData->fname != NULL) {
                parser_errmsg("omfile: both \"file\" and \"dynafile\" set, will use dynafile");
            }
            pData->fname = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            CODE_STD_STRING_REQUESTnewActInst(2);
            pData->bDynamicName = 1;
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "sig.provider")) {
            pData->sigprovName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "cry.provider")) {
            pData->cryprovName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "closetimeout")) {
            pData->iCloseTimeout = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "rotation.sizelimit")) {
            pData->iSizeLimit = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "rotation.sizelimitcommand")) {
            pData->pszSizeLimitCmd = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf(
                "omfile: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }

    if (pData->fname == NULL || *pData->fname == '\0') {
        parser_errmsg(
            "omfile: either the \"file\" or "
            "\"dynafile\" parameter must be given");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    int allWhiteSpace = 1;
    for (const char *p = (const char *)pData->fname; *p; ++p) {
        if (!isspace(*p)) {
            allWhiteSpace = 0;
            break;
        }
    }
    if (allWhiteSpace) {
        parser_errmsg(
            "omfile: \"file\" or \"dynafile\" parameter "
            "consist only of whitespace - this is not permitted");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (!strcmp((const char *)pData->fname, "/dev/null")) {
        pData->isDevNull = 1;
    }

    if (pData->sigprovName != NULL) {
        initSigprov(pData, lst);
    }

    if (pData->cryprovName != NULL) {
        CHKiRet(initCryprov(pData, lst));
    }

    tplToUse = ustrdup((pData->tplName == NULL) ? getDfltTpl() : pData->tplName);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));
    pData->iNumTpls = 1;

    if (pData->bDynamicName) {
        /* "filename" is actually a template name, we need this as string 1. So let's add it
         * to the pOMSR. -- rgerhards, 2007-07-27
         */
        CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->fname), OMSR_NO_RQD_TPL_OPTS));
        pData->iNumTpls = 2;
        // TODO: create unified code for this (legacy+v6 system)
        /* we now allocate the cache table */
        CHKmalloc(pData->dynCache =
                      (dynaFileCacheEntry **)calloc(pData->iDynaFileCacheSize, sizeof(dynaFileCacheEntry *)));
        pData->iCurrElt = -1; /* no current element */
    }
    // TODO: add	pData->iSizeLimit = 0; /* default value, use outchannels to configure! */
    setupInstStatsCtrs(pData);

    if (pData->iCloseTimeout == -1) { /* unset? */
        pData->iCloseTimeout = (pData->bDynamicName) ? 10 : 0;
    }

    snprintf(pData->janitorID, sizeof(pData->janitorID), "omfile:%sfile:%s:%p", (pData->bDynamicName) ? "dyna" : "",
             pData->fname, pData);
    pData->janitorID[sizeof(pData->janitorID) - 1] = '\0'; /* just in case... */

    if (pData->iCloseTimeout > 0) janitorAddEtry(janitorCB, pData->janitorID, pData);

    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
    uchar fname[MAXFNAME];
    CODESTARTparseSelectorAct;
    /* Note: the indicator sequence permits us to use '$' to signify
     * outchannel, what otherwise is not possible due to truely
     * unresolvable grammar conflicts (*this time no way around*).
     * rgerhards, 2011-07-09
     */
    if (!strncmp((char *)p, ":omfile:", sizeof(":omfile:") - 1)) {
        p += sizeof(":omfile:") - 1;
    }
    if (!(*p == '$' || *p == '?' || *p == '/' || *p == '.' || *p == '-')) ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);

    CHKiRet(createInstance(&pData));

    if (*p == '-') {
        pData->bSyncFile = 0;
        p++;
    } else {
        pData->bSyncFile = cs.bEnableSync;
    }
    pData->iSizeLimit = 0; /* default value, use outchannels to configure! */

    switch (*p) {
        case '$':
            CODE_STD_STRING_REQUESTparseSelectorAct(1) pData->iNumTpls = 1;
            /* rgerhards 2005-06-21: this is a special setting for output-channel
             * definitions. In the long term, this setting will probably replace
             * anything else, but for the time being we must co-exist with the
             * traditional mode lines.
             * rgerhards, 2007-07-24: output-channels will go away. We keep them
             * for compatibility reasons, but seems to have been a bad idea.
             */
            CHKiRet(cflineParseOutchannel(pData, p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS));
            pData->bDynamicName = 0;
            break;

        case '?': /* This is much like a regular file handle, but we need to obtain
                   * a template name. rgerhards, 2007-07-03
                   */
            CODE_STD_STRING_REQUESTparseSelectorAct(2) pData->iNumTpls = 2;
            ++p; /* eat '?' */
            CHKiRet(cflineParseFileName(p, fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, getDfltTpl()));
            pData->fname = ustrdup(fname);
            pData->bDynamicName = 1;
            pData->iCurrElt = -1; /* no current element */
            /* "filename" is actually a template name, we need this as string 1. So let's add it
             * to the pOMSR. -- rgerhards, 2007-07-27
             */
            CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->fname), OMSR_NO_RQD_TPL_OPTS));
            /* we now allocate the cache table */
            CHKmalloc(pData->dynCache =
                          (dynaFileCacheEntry **)calloc(cs.iDynaFileCacheSize, sizeof(dynaFileCacheEntry *)));
            break;

        case '/':
        case '.':
            CODE_STD_STRING_REQUESTparseSelectorAct(1) pData->iNumTpls = 1;
            CHKiRet(cflineParseFileName(p, fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, getDfltTpl()));
            pData->fname = ustrdup(fname);
            pData->bDynamicName = 0;
            break;
        default:
            ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    /* freeze current paremeters for this action */
    pData->iDynaFileCacheSize = cs.iDynaFileCacheSize;
    pData->fCreateMode = cs.fCreateMode;
    pData->fDirCreateMode = cs.fDirCreateMode;
    pData->bCreateDirs = cs.bCreateDirs;
    pData->bFailOnChown = cs.bFailOnChown;
    pData->fileUID = cs.fileUID;
    pData->fileGID = cs.fileGID;
    pData->dirUID = cs.dirUID;
    pData->dirGID = cs.dirGID;
    pData->iZipLevel = cs.iZipLevel;
    pData->bFlushOnTXEnd = cs.bFlushOnTXEnd;
    pData->iIOBufSize = (int)cs.iIOBufSize;
    pData->iFlushInterval = cs.iFlushInterval;
    pData->bUseAsyncWriter = cs.bUseAsyncWriter;
    pData->bAddLF = cs.bAddLF;
    pData->bVeryRobustZip = 0; /* cannot be specified via legacy conf */
    pData->iCloseTimeout = 0; /* cannot be specified via legacy conf */
    setupInstStatsCtrs(pData);
    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/**
 * @brief Resets configuration variables for the omfile module to default values.
 *
 * This function is used to revert module-global configuration settings
 * to their initial states, typically during `resetconfigvariables` processing.
 *
 * @param pp Unused pointer, kept for compatibility.
 * @param pVal Unused pointer, kept for compatibility.
 * @return RS_RET_OK always.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    cs.fileUID = -1;
    cs.fileGID = -1;
    cs.dirUID = -1;
    cs.dirGID = -1;
    cs.bFailOnChown = 1;
    cs.iDynaFileCacheSize = 10;
    cs.fCreateMode = 0644;
    cs.fDirCreateMode = 0700;
    cs.bCreateDirs = 1;
    cs.bEnableSync = 0;
    cs.iZipLevel = 0;
    cs.bFlushOnTXEnd = FLUSHONTX_DFLT;
    cs.iIOBufSize = IOBUF_DFLT_SIZE;
    cs.iFlushInterval = FLUSH_INTRVL_DFLT;
    cs.bUseAsyncWriter = USE_ASYNCWRITER_DFLT;
    cs.bAddLF = 0;
    free(pszFileDfltTplName);
    pszFileDfltTplName = NULL;
    return RS_RET_OK;
}


BEGINdoHUP
    CODESTARTdoHUP;
    pthread_mutex_lock(&pData->mutWrite);
    if (pData->bDynamicName) {
        dynaFileFreeCacheEntries(pData);
    } else {
        if (pData->pStrm != NULL) {
            closeFile(pData);
        }
    }
    pthread_mutex_unlock(&pData->mutWrite);
ENDdoHUP


BEGINmodExit
    CODESTARTmodExit;
    objRelease(strm, CORE_COMPONENT);
    objRelease(statsobj, CORE_COMPONENT);
    DESTROY_ATOMIC_HELPER_MUT(mutClock);
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMODTX_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_doHUP
ENDqueryEtryPt


BEGINmodInit(File)
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr INITLegCnfVars;
    CHKiRet(objUse(strm, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));

    INIT_ATOMIC_HELPER_MUT(mutClock);

    INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
    DBGPRINTF("omfile: %susing transactional output interface.\n", bCoreSupportsBatching ? "" : "not ");
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"dynafilecachesize", 0, eCmdHdlrInt, setDynaFileCacheSize, NULL,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileziplevel", 0, eCmdHdlrInt, NULL, &cs.iZipLevel, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileflushinterval", 0, eCmdHdlrInt, NULL, &cs.iFlushInterval,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileasyncwriting", 0, eCmdHdlrBinary, NULL, &cs.bUseAsyncWriter,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileflushontxend", 0, eCmdHdlrBinary, NULL, &cs.bFlushOnTXEnd,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileaddlf", 0, eCmdHdlrBinary, NULL, &cs.bAddLF, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileiobuffersize", 0, eCmdHdlrSize, NULL, &cs.iIOBufSize,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirowner", 0, eCmdHdlrUID, NULL, &cs.dirUID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirownernum", 0, eCmdHdlrInt, NULL, &cs.dirUID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirgroup", 0, eCmdHdlrGID, NULL, &cs.dirGID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirgroupnum", 0, eCmdHdlrInt, NULL, &cs.dirGID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"fileowner", 0, eCmdHdlrUID, NULL, &cs.fileUID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"fileownernum", 0, eCmdHdlrInt, NULL, &cs.fileUID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"filegroup", 0, eCmdHdlrGID, NULL, &cs.fileGID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"filegroupnum", 0, eCmdHdlrInt, NULL, &cs.fileGID, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"dircreatemode", 0, eCmdHdlrFileCreateMode, NULL, &cs.fDirCreateMode,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"filecreatemode", 0, eCmdHdlrFileCreateMode, NULL, &cs.fCreateMode,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(
        omsdRegCFSLineHdlr((uchar *)"createdirs", 0, eCmdHdlrBinary, NULL, &cs.bCreateDirs, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"failonchownfailure", 0, eCmdHdlrBinary, NULL, &cs.bFailOnChown,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileforcechown", 0, eCmdHdlrGoneAway, NULL, NULL, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionfileenablesync", 0, eCmdHdlrBinary, NULL, &cs.bEnableSync,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionfiledefaulttemplate", 0, eCmdHdlrGetWord, setLegacyDfltTpl, NULL,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit
