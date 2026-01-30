/* The zlibwrap object.
 *
 * This is an rsyslog object wrapper around zlib.
 *
 * Copyright 2009-2022 Rainer Gerhards and Adiscon GmbH.
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
#include <zlib.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "stream.h"
#include "zlibw.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP;

/* static data */
DEFobjStaticHelpers;


/* ------------------------------ methods ------------------------------ */

/* zlib make strong use of macros for its interface functions, so we can not simply
 * pass function pointers to them. Instead, we create very small wrappers which call
 * the relevant entry points.
 */

static int myDeflateInit(z_streamp strm, int level) {
    return deflateInit(strm, level);
}

static int myDeflateInit2(z_streamp strm, int level, int method, int windowBits, int memLevel, int strategy) {
    return deflateInit2(strm, level, method, windowBits, memLevel, strategy);
}

static int myDeflateEnd(z_streamp strm) {
    return deflateEnd(strm);
}

static int myDeflate(z_streamp strm, int flush) {
    return deflate(strm, flush);
}

/* finish zlib buffer */
static rsRetVal doCompressFinish(strm_t *pThis, rsRetVal (*strmPhysWrite)(strm_t *pThis, uchar *pBuf, size_t lenBuf)) {
    int zRet; /* zlib return state */
    DEFiRet;
    unsigned outavail;
    assert(pThis != NULL);

    assert(pThis->compressionDriver == STRM_COMPRESS_ZIP);

    if (!pThis->bzInitDone) goto done;

    pThis->zstrm.avail_in = 0;
    /* run deflate() on buffer until everything has been compressed */
    do {
        DBGPRINTF("in deflate() loop, avail_in %d, total_in %ld\n", pThis->zstrm.avail_in, pThis->zstrm.total_in);
        pThis->zstrm.avail_out = pThis->sIOBufSize;
        pThis->zstrm.next_out = pThis->pZipBuf;
        zRet = deflate(&pThis->zstrm, Z_FINISH); /* no bad return value */
        DBGPRINTF("after deflate, ret %d, avail_out %d\n", zRet, pThis->zstrm.avail_out);
        outavail = pThis->sIOBufSize - pThis->zstrm.avail_out;
        if (outavail != 0) {
            CHKiRet(strmPhysWrite(pThis, (uchar *)pThis->pZipBuf, outavail));
        }
    } while (pThis->zstrm.avail_out == 0);

finalize_it:
    zRet = deflateEnd(&pThis->zstrm);
    if (zRet != Z_OK) {
        LogError(0, RS_RET_ZLIB_ERR, "error %d returned from zlib/deflateEnd()", zRet);
    }

    pThis->bzInitDone = 0;
done:
    RETiRet;
}

/* write the output buffer in zip mode
 * This means we compress it first and then do a physical write.
 * Note that we always do a full deflateInit ... deflate ... deflateEnd
 * sequence. While this is not optimal, we need to do it because we need
 * to ensure that the file is readable even when we are aborted. Doing the
 * full sequence brings us as far towards this goal as possible (and not
 * doing it would be a total failure). It may be worth considering to
 * add a config switch so that the user can decide the risk he is ready
 * to take, but so far this is not yet implemented (not even requested ;)).
 * rgerhards, 2009-06-04
 */
static rsRetVal doStrmWrite(strm_t *pThis,
                            uchar *pBuf,
                            size_t lenBuf,
                            const int bFlush,
                            rsRetVal (*strmPhysWrite)(strm_t *pThis, uchar *pBuf, size_t lenBuf)) {
    int zRet; /* zlib return state */
    DEFiRet;
    unsigned outavail = 0;
    assert(pThis != NULL);
    assert(pBuf != NULL);

    assert(pThis->compressionDriver == STRM_COMPRESS_ZIP);

    if (!pThis->bzInitDone) {
        /* allocate deflate state */
        pThis->zstrm.zalloc = Z_NULL;
        pThis->zstrm.zfree = Z_NULL;
        pThis->zstrm.opaque = Z_NULL;
        /* see note in file header for the params we use with deflateInit2() */
        zRet = deflateInit2(&pThis->zstrm, pThis->iZipLevel, Z_DEFLATED, 31, 9, Z_DEFAULT_STRATEGY);
        if (zRet != Z_OK) {
            LogError(0, RS_RET_ZLIB_ERR, "error %d returned from zlib/deflateInit2()", zRet);
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }
        pThis->bzInitDone = RSTRUE;
    }

    /* now doing the compression */
    pThis->zstrm.next_in = (Bytef *)pBuf;
    pThis->zstrm.avail_in = lenBuf;
    /* run deflate() on buffer until everything has been compressed */
    do {
        DBGPRINTF("in deflate() loop, avail_in %d, total_in %ld, bFlush %d\n", pThis->zstrm.avail_in,
                  pThis->zstrm.total_in, bFlush);
        pThis->zstrm.avail_out = pThis->sIOBufSize;
        pThis->zstrm.next_out = pThis->pZipBuf;
        zRet = deflate(&pThis->zstrm, bFlush ? Z_SYNC_FLUSH : Z_NO_FLUSH); /* no bad return value */
        DBGPRINTF("after deflate, ret %d, avail_out %d, to write %d\n", zRet, pThis->zstrm.avail_out, outavail);
        if (zRet != Z_OK) {
            LogError(0, RS_RET_ZLIB_ERR, "error %d returned from zlib/Deflate()", zRet);
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }
        outavail = pThis->sIOBufSize - pThis->zstrm.avail_out;
        if (outavail != 0) {
            CHKiRet(strmPhysWrite(pThis, (uchar *)pThis->pZipBuf, outavail));
        }
    } while (pThis->zstrm.avail_out == 0);

finalize_it:
    if (pThis->bzInitDone && pThis->bVeryReliableZip) {
        doCompressFinish(pThis, strmPhysWrite);
    }
    RETiRet;
}
/* destruction of caller's zlib ressources - a dummy for us */
static rsRetVal zlib_Destruct(ATTR_UNUSED strm_t *pThis) {
    return RS_RET_OK;
}

/* queryInterface function
 * rgerhards, 2008-03-05
 */
BEGINobjQueryInterface(zlibw)
    CODESTARTobjQueryInterface(zlibw);
    if (pIf->ifVersion != zlibwCURR_IF_VERSION) { /* check for current version, increment on each change */
        ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
    }

    /* ok, we have the right interface, so let's fill it
     * Please note that we may also do some backwards-compatibility
     * work here (if we can support an older interface version - that,
     * of course, also affects the "if" above).
     */
    pIf->DeflateInit = myDeflateInit;
    pIf->DeflateInit2 = myDeflateInit2;
    pIf->Deflate = myDeflate;
    pIf->DeflateEnd = myDeflateEnd;
    pIf->doStrmWrite = doStrmWrite;
    pIf->doCompressFinish = doCompressFinish;
    pIf->Destruct = zlib_Destruct;
finalize_it:
ENDobjQueryInterface(zlibw)


/* Initialize the zlibw class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(zlibw, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
    /* request objects we use */

    /* set our own handlers */
ENDObjClassInit(zlibw)


/* Exit the class. */
BEGINObjClassExit(zlibw, OBJ_IS_LOADABLE_MODULE) /* class, version */
    CODESTARTObjClassExit(zlibw);
ENDObjClassExit(zlibw)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
    CODESTARTmodExit;
    zlibwClassExit();
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_LIB_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

    CHKiRet(zlibwClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
    /* Initialize all classes that are in our module - this includes ourselfs */
ENDmodInit
/* vi:set ai:
 */
