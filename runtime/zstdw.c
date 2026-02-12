/* The zstdw object.
 *
 * This is an rsyslog object wrapper around zstd.
 *
 * Copyright 2022 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
#include <string.h>
#include <assert.h>
#include <zstd.h>

#include "rsyslog.h"
#include "errmsg.h"
#include "stream.h"
#include "module-template.h"
#include "obj.h"
#include "zstdw.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP;

/* static data */
DEFobjStaticHelpers;


/* finish buffer, to be called before closing the zstd file. */
static rsRetVal zstd_doCompressFinish(strm_t *pThis,
                                      rsRetVal (*strmPhysWrite)(strm_t *pThis, uchar *pBuf, size_t lenBuf)) {
    size_t remaining = 0;
    DEFiRet;
    assert(pThis != NULL);

    if (!pThis->bzInitDone) goto done;

    char dummybuf; /* not sure if we can pass in NULL as buffer address in this special case */
    ZSTD_inBuffer input = {&dummybuf, 0, 0};

    do {
        ZSTD_outBuffer output = {pThis->pZipBuf, pThis->sIOBufSize, 0};
        remaining = ZSTD_compressStream2(pThis->zstd.cctx, &output, &input, ZSTD_e_end);
        if (ZSTD_isError(remaining)) {
            LogError(0, RS_RET_ZLIB_ERR, "error returned from ZSTD_compressStream2(): %s\n",
                     ZSTD_getErrorName(remaining));
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }

        CHKiRet(strmPhysWrite(pThis, (uchar *)pThis->pZipBuf, output.pos));

    } while (remaining != 0);

finalize_it:
done:
    RETiRet;
}


static rsRetVal zstd_doStrmWrite(strm_t *pThis,
                                 uchar *const pBuf,
                                 const size_t lenBuf,
                                 const int bFlush,
                                 rsRetVal (*strmPhysWrite)(strm_t *pThis, uchar *pBuf, size_t lenBuf)) {
    DEFiRet;
    assert(pThis != NULL);
    assert(pBuf != NULL);
    if (!pThis->bzInitDone) {
        pThis->zstd.cctx = (void *)ZSTD_createCCtx();
        if (pThis->zstd.cctx == NULL) {
            LogError(0, RS_RET_ZLIB_ERR,
                     "error creating zstd context (ZSTD_createCCtx failed, "
                     "that's all we know");
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }

        ZSTD_CCtx_setParameter(pThis->zstd.cctx, ZSTD_c_compressionLevel, pThis->iZipLevel);
        ZSTD_CCtx_setParameter(pThis->zstd.cctx, ZSTD_c_checksumFlag, 1);
        if (pThis->zstd.num_wrkrs > 0) {
            ZSTD_CCtx_setParameter(pThis->zstd.cctx, ZSTD_c_nbWorkers, pThis->zstd.num_wrkrs);
        }
        pThis->bzInitDone = RSTRUE;
    }

    /* now doing the compression */
    ZSTD_inBuffer input = {pBuf, lenBuf, 0};

    // This following needs to be configurable? It's possibly sufficient to use e_flush
    // only, as this can also be controlled by veryRobustZip. However, testbench will than
    // not be able to check when all file lines are complete.
    ZSTD_EndDirective const mode = bFlush ? ZSTD_e_flush : ZSTD_e_continue;
    size_t remaining;
    do {
        ZSTD_outBuffer output = {pThis->pZipBuf, 128, 0};
        remaining = ZSTD_compressStream2(pThis->zstd.cctx, &output, &input, mode);
        if (ZSTD_isError(remaining)) {
            LogError(0, RS_RET_ZLIB_ERR, "error returned from ZSTD_compressStream2(): %s",
                     ZSTD_getErrorName(remaining));
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }

        CHKiRet(strmPhysWrite(pThis, (uchar *)pThis->pZipBuf, output.pos));

    } while (mode == ZSTD_e_end ? (remaining != 0) : (input.pos != input.size));

finalize_it:
    if (pThis->bzInitDone && pThis->bVeryReliableZip) {
        zstd_doCompressFinish(pThis, strmPhysWrite);
    }
    RETiRet;
}

/* destruction of caller's zstd ressources */
static rsRetVal zstd_Destruct(strm_t *const pThis) {
    DEFiRet;
    assert(pThis != NULL);

    if (!pThis->bzInitDone) goto done;

    const int result = ZSTD_freeCCtx(pThis->zstd.cctx);
    if (ZSTD_isError(result)) {
        LogError(0, RS_RET_ZLIB_ERR, "error from ZSTD_freeCCtx(): %s", ZSTD_getErrorName(result));
    }

    pThis->bzInitDone = 0;
done:
    RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-03-05
 */
BEGINobjQueryInterface(zstdw)
    CODESTARTobjQueryInterface(zstdw);
    if (pIf->ifVersion != zstdwCURR_IF_VERSION) { /* check for current version, increment on each change */
        ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
    }
    pIf->doStrmWrite = zstd_doStrmWrite;
    pIf->doCompressFinish = zstd_doCompressFinish;
    pIf->Destruct = zstd_Destruct;
finalize_it:
ENDobjQueryInterface(zstdw)


/* Initialize the zstdw class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(zstdw, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
ENDObjClassInit(zstdw)


/* Exit the class. */
BEGINObjClassExit(zstdw, OBJ_IS_LOADABLE_MODULE) /* class, version */
    CODESTARTObjClassExit(zstdw);
ENDObjClassExit(zstdw)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
    CODESTARTmodExit;
    zstdwClassExit();
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_LIB_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CHKiRet(zstdwClassInit(pModInfo));
ENDmodInit
