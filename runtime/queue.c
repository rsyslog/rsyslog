/* queue.c
 *
 * This file implements the queue object and its several queueing methods.
 *
 * File begun on 2008-01-03 by RGerhards
 *
 * There is some in-depth documentation available in doc/dev_queue.html
 * (and in the web doc set on https://www.rsyslog.com/doc/). Be sure to read it
 * if you are getting acquainted to the object.
 *
 * NOTE: as of 2009-04-22, I have begin to remove the qqueue* prefix from static
 * function names - this makes it really hard to read and does not provide much
 * benefit, at least I (now) think so...
 *
 * Copyright 2008-2025 Rainer Gerhards and Adiscon GmbH.
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

/**
 * @file queue.c
 * @brief This file implements the rsyslog queueing subsystem.
 *
 * @section queue_maintenance Important Note on Maintenance
 *
 * This header comment contains critical information on the system's
 * architecture and design philosophy. It is essential that this comment, and
 * all other relevant documentation, be **updated whenever architectural or
 * other significant changes are made to the queueing subsystem.**
 *
 * Maintaining synchronization between code and documentation is vital for
 * long-term project health, developer onboarding, and to enable automated
 * tools and AI agents to accurately analyze the codebase and detect potential
 * issues arising from undocumented changes. This documentation reflects the
 * state of the system as of mid-2025, based on a battle-proven design that
 * originated circa 2004.
 *
 * @section queue_architecture Architectural Overview and Design Philosophy
 *
 * The rsyslog queueing system is a fundamental component for providing both
 * performance and reliability. It is built on a powerful abstraction: a queue
 * can be placed at two key points in the message processing pipeline:
 *
 * 1.  **Ruleset Queue (Main Message Queue):** Each ruleset has a single queue
 * that buffers messages received from inputs *before* they are processed
 * by the ruleset's filters. This decouples message ingestion from filter
 * processing, allowing rsyslog to handle massive input bursts without
 * losing messages. The queue for the default ruleset is often referred
 * to by its historical name, the "main message queue".
 *
 * 2.  **Action Queue:** Each action within a ruleset can have its own dedicated
 * queue. This decouples the filter engine from the output action (e.g.,
 * writing to a file or sending over the network).
 *
 * This system's design is a testament to operator-centric control, providing
 * a sophisticated toolkit of compromises. This contrasts sharply with modern
 * "WAL-only" log shippers, making rsyslog uniquely versatile.
 *
 *
 * @subsection queue_types Queue Types
 *
 * Rsyslog offers multiple queue types, each with a specific performance and
 * reliability profile. They are listed here from most lightweight to most
 * robust.
 *
 * 1.  **Direct (The "No-Queue" Queue)**
 * - **Behavior:** The default for all **action queues**. No buffering occurs. The
 * worker thread from the parent queue (usually the ruleset's queue)
 * executes the action's logic directly.
 * - **Use Case:** For fast, non-blocking, local actions (e.g., `omfile`).
 * - **Warning:** If a Direct-queued action blocks, it stalls the worker
 * thread, potentially halting all processing for that worker.
 *
 * 2.  **In-Memory (LinkedList and FixedArray)**
 * - **Behavior:** Buffers messages in RAM. Extremely fast but offers no
 * persistence across restarts.
 * - **Sub-Types:**
 * - `LinkedList`: The recommended default for most in-memory queues. It is
 * memory-efficient, allocating space only for messages it holds.
 * - `FixedArray`: A legacy option that pre-allocates a static array of
 * pointers. It can be slightly faster under constant load but is
 * less memory-efficient. It remains the default for ruleset queues.
 * - **Use Case:** High-performance buffering where a potential loss of
 * in-flight messages on crash is acceptable.
 *
 * 3.  **Disk (The "Pure-Disk" Queue)**
 * - **Behavior:** Writes every single message to a disk-based queue structure
 * before acknowledging the enqueue operation. This queue provides a
 * **"Limited Duplication"** guarantee, not a simple "at-least-once".
 * - **The `.qi` Checkpoint File:** The queue's state (read/write pointers)
 * is persisted in a `.qi` file. The `queue.checkpointInterval` parameter
 * dictates how often this file is updated, allowing the user to tune
 * the trade-off between I/O performance and duplication risk. A value
 * of `1` provides near-exactly-once delivery, essential for "dumb"
 * (non-deduplicating) receivers.
 * - **Use Case:** For audit-grade logging chains where no message loss can
 * be tolerated, even in the case of a power failure or ungraceful shutdown.
 *
 * 4.  **Disk-Assisted (DA) (The Hybrid "Best-of-Both-Worlds" Queue)**
 * - **Behavior:** This is the most sophisticated queue type. It acts as a
 * multi-stage defense system against data loss.
 * - **Stage 1: In-Memory First:** By default, it operates as a high-speed
 * `LinkedList` queue with zero disk I/O.
 * - **Stage 2: Disk Spooling:** If the in-memory queue exceeds its
 * `highwatermark` (e.g., due to downstream backpressure), it seamlessly
 * activates its internal **Disk Queue** and begins spooling messages
 * to disk. This provides resilience to transient failures without the
 * constant performance penalty of a pure Disk queue. The disk portion
 * operates with its own "Limited Duplication" guarantee.
 * - **Stage 3: Load Shedding:** If all buffers (memory and disk) are full,
 * the queue hits the `queue.discardMark`. It can then begin to discard
 * messages based on severity (`queue.discardSeverity`), preserving
 * critical logs during a total system overload.
 * - **Use Case:** The recommended choice for any potentially unreliable or
 * slow action, or for a ruleset queue that needs to survive downstream
 * outages.
 *
 * 5.  **Segmented Disk**
 * - **Behavior:** A pure-disk queue backed by a log-structured segmented
 * store. It uses one serial writer, sealed segment files, and durable
 * per-segment commit offsets. It can be selected as the disk store used by
 * disk-assisted (DA) queues via DA engine/store selection.
 * - **Use Case:** Durable queueing with segmented-store mechanics, including
 * DA deployments that use the segmented disk store.
 *
 *
 * @subsection comparison_to_wal Rsyslog's "Bounded Queue" vs. a WAL's "Unbounded Stream"
 *
 * It is critical to understand that rsyslog's disk-based queues implement a
 * **Bounded FIFO Queue**, which is architecturally different from the
 * **Unbounded Stream** model of a Write-Ahead Log (WAL) found in tools like
 * Fluent Bit or Vector.
 *
 * - **Rsyslog's Model:** The `.qi` file checkpoints the queue's *structure*,
 * containing two primary tuples: `write_ptr = (segment, offset)` and
 * `read_ptr = (segment, offset)`. This defines the queue's boundaries.
 * Consumption is a destructive action that advances the `read_ptr`. On a
 * graceful restart (e.g., K8s `SIGTERM`), DA queues flush memory to disk,
 * ensuring **zero data loss**. On a crash, only the `checkpointInterval`-worth
 * of messages are at risk of replay. This fine-grained control makes it safe
 * for both smart and dumb receivers. **Note:** A known operational risk is
 * that the current implementation does not gracefully handle a missing or
 * corrupt `.qi` file in conjunction with pre-existing queue segment files.
 * This can lead to startup failures or inconsistent state and is a top
 * priority for future reliability enhancements.
 *
 *
 * - **WAL Model:** A WAL is a simple, append-only log. The checkpoint is just
 * a consumer's *offset*. On restart, a WAL-based shipper replays *all data*
 * from the last offset, which can be massive. This model mandates a smart,
 * idempotent receiver and is fundamentally unsafe for dumb endpoints.*
 * @subsection naming_convention Historical Naming: queue vs. qqueue
 *
 * Throughout the code, you will see types and variables prefixed with `qqueue`
 * (e.g., `qqueue_t`). This is the result of a historical name change.
 * Originally, these were named `queue`, but this caused symbol clashes on some
 * platforms (e.g., AIX) where `queue` is a reserved name in system libraries.
 * The name was changed to `qqueue` ("queue object for the queueing subsystem")
 * to ensure portability.
 *
 *
 * @section conclusion Summary for Developers
 *
 * When working with this code, remember that you are not dealing with a simple
 * log appender. You are maintaining a transactional, persistent FIFO queue.
 * The logic surrounding the `.qi` file, segment files, and the read/write
 * pointers is designed to provide robust, tunable delivery guarantees that are
 * a core feature of rsyslog. This makes it more versatile than pure WAL-based
 * log shippers.
 *
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h> /* required for HP UX */
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>

#include "rsyslog.h"
#include "queue.h"
#include "queue_local.h"
#include "queue_local_stats.h"
#include "queue_lease.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "obj.h"
#include "wtp.h"
#include "wti.h"
#include "msg.h"
#include "obj.h"
#include "atomic.h"
#include "errmsg.h"
#include "datetime.h"
#include "unicode-helper.h"
#include "statsobj.h"
#include "parserif.h"
#include "rsconf.h"
#include "ruleset.h"

#ifdef OS_SOLARIS
    #include <sched.h>
#endif

/* static data */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl) DEFobjCurrIf(strm) DEFobjCurrIf(datetime) DEFobjCurrIf(statsobj)

#if __GNUC__ >= 8
    #pragma GCC diagnostic ignored "-Wcast-function-type"  // TODO: investigate further!
#endif /* if __GNUC__ >= 8 */

#ifdef ENABLE_IMDIAG
    unsigned int iOverallQueueSize = 0;
#endif

#define OVERSIZE_QUEUE_WATERMARK 500000 /* when is a queue considered to be "overly large"? */
#define MAX_DISK_QUEUE_FILES 10000000 /* maximum file number for disk queues */
#define DISKQUEUE_CORRUPTION_RESYNC_MAX_BYTES (1024 * 1024)


/* forward-definitions */
static rsRetVal doEnqSingleObj(qqueue_t *pThis, flowControl_t flowCtlType, smsg_t *pMsg);
static rsRetVal doEnqSingleObjContext(qqueue_t *pThis, flowControl_t flowCtlType, smsg_t *pMsg, int sourceRetry);
static rsRetVal qqueueChkPersist(qqueue_t *pThis, int nUpdates);
static rsRetVal RateLimiter(qqueue_t *pThis);
static rsRetVal qqueueChkStopWrkrDA(qqueue_t *pThis);
static rsRetVal GetDeqBatchSize(qqueue_t *pThis, int *pVal);
static rsRetVal ConsumerDA(qqueue_t *pThis, wti_t *pWti);
static rsRetVal batchProcessed(qqueue_t *pThis, wti_t *pWti);
static rsRetVal qqueueMultiEnqObjNonDirect(qqueue_t *pThis, multi_submit_t *pMultiSub);
static rsRetVal qqueueMultiEnqObjDirect(qqueue_t *pThis, multi_submit_t *pMultiSub);
static rsRetVal qAddDirect(qqueue_t *pThis, smsg_t *pMsg);
static rsRetVal qDestructDirect(qqueue_t __attribute__((unused)) * pThis);
static rsRetVal qConstructDirect(qqueue_t __attribute__((unused)) * pThis);
static rsRetVal qConstructDisk(qqueue_t *pThis);
static rsRetVal qDestructDisk(qqueue_t *pThis);
static rsRetVal qConstructSegDisk(qqueue_t *pThis);
static rsRetVal qDestructSegDisk(qqueue_t *pThis);
static rsRetVal qAddSegDisk(qqueue_t *pThis, smsg_t *pMsg);
static rsRetVal qDeqBatchSegDisk(qqueue_t *pThis, batch_t *batch, int max, int *skipped);
static rsRetVal qCompleteBatchSegDisk(qqueue_t *pThis, batch_t *batch, int *committed, int *retried);
rsRetVal qqueueSetSpoolDir(qqueue_t *pThis, uchar *pszSpoolDir, int lenSpoolDir);
static rsRetVal handleReadSeekError(rsRetVal seekRet, qqueue_t *pThis, const char *streamName, sbool *pReadSeekFailed);
static void alignReadDeqToWrite(qqueue_t *pThis);
static void recoverFromInvalidQi(qqueue_t *pThis, int wr_fd, int64_t wr_offs);
static void qqueueDestroyDiskStreams(qqueue_t *pThis);
static rsRetVal qqueueSwitchToInMemoryEmergency(qqueue_t *pThis);
static rsRetVal qqueueResetDiskQueueAfterCorruption(qqueue_t *pThis);
static rsRetVal msgConstructFromVoid(void **ppThis);
static rsRetVal msgDeserializeFromVoid(void *pObj, strm_t *pStrm);

/* some constants for queuePersist () */
#define QUEUE_CHECKPOINT 1
#define QUEUE_NO_CHECKPOINT 0

/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfpdescr[] = {{"queue.filename", eCmdHdlrGetWord, 0},
                                           {"queue.spooldirectory", eCmdHdlrGetWord, 0},
                                           {"queue.size", eCmdHdlrSize, 0},
                                           {"queue.dequeuebatchsize", eCmdHdlrInt, 0},
                                           {"queue.mindequeuebatchsize", eCmdHdlrInt, 0},
                                           {"queue.mindequeuebatchsize.timeout", eCmdHdlrInt, 0},
                                           {"queue.maxdiskspace", eCmdHdlrSize, 0},
                                           {"queue.highwatermark", eCmdHdlrInt, 0},
                                           {"queue.lowwatermark", eCmdHdlrInt, 0},
                                           {"queue.fulldelaymark", eCmdHdlrInt, 0},
                                           {"queue.lightdelaymark", eCmdHdlrInt, 0},
                                           {"queue.discardmark", eCmdHdlrInt, 0},
                                           {"queue.discardseverity", eCmdHdlrFacility, 0},
                                           {"queue.checkpointinterval", eCmdHdlrInt, 0},
                                           {"queue.syncqueuefiles", eCmdHdlrBinary, 0},
                                           {"queue.type", eCmdHdlrQueueType, 0},
                                           {"queue.diskqueuetype", eCmdHdlrGetWord, 0},
                                           {"queue.diskqueueautoupgrade", eCmdHdlrBinary, 0},
                                           {"queue.diskqueueidletimeout", eCmdHdlrInt, 0},
                                           {"queue.workerthreads", eCmdHdlrPositiveInt, 0},
                                           {"queue.timeoutshutdown", eCmdHdlrInt, 0},
                                           {"queue.timeoutactioncompletion", eCmdHdlrInt, 0},
                                           {"queue.timeoutenqueue", eCmdHdlrInt, 0},
                                           {"queue.timeoutworkerthreadshutdown", eCmdHdlrInt, 0},
                                           {"queue.workerthreadminimummessages", eCmdHdlrInt, 0},
                                           {"queue.maxfilesize", eCmdHdlrSize, 0},
                                           {"queue.saveonshutdown", eCmdHdlrBinary, 0},
                                           {"queue.dequeueslowdown", eCmdHdlrInt, 0},
                                           {"queue.dequeuetimebegin", eCmdHdlrInt, 0},
                                           {"queue.dequeuetimeend", eCmdHdlrInt, 0},
                                           {"queue.cry.provider", eCmdHdlrGetWord, 0},
                                           {"queue.samplinginterval", eCmdHdlrInt, 0},
                                           {"queue.takeflowctlfrommsg", eCmdHdlrBinary, 0},
                                           {"queue.mutexcontentionstats", eCmdHdlrBinary, 0},
                                           {"queue.oncorruption", eCmdHdlrGetWord, 0},
                                           {"queue.scope", eCmdHdlrGetWord, 0},
                                           {"queue.local.frontendsize", eCmdHdlrInt, 0},
                                           {"queue.local.maxfrontends", eCmdHdlrInt, 0},
                                           {"queue.local.frontendstats", eCmdHdlrBinary, 0},
                                           {"queue.local.helperbatchsize", eCmdHdlrInt, 0}};
static struct cnfparamblk pblk = {CNFPARAMBLK_VERSION, sizeof(cnfpdescr) / sizeof(struct cnfparamdescr), cnfpdescr};

/* support to detect duplicate queue file names */
struct queue_filename {
    struct queue_filename *next;
    const char *dirname;
    const char *filename;
};
struct queue_filename *queue_filename_root = NULL;

/* debug aid */
#if 0
static inline void displayBatchState(batch_t *pBatch)
{
	int i;
	for(i = 0 ; i < pBatch->nElem ; ++i) {
		DBGPRINTF("displayBatchState %p[%d]: %d\n", pBatch, i, pBatch->eltState[i]);
	}
}
#endif
static rsRetVal qqueuePersist(qqueue_t *pThis, int bIsCheckpoint);
static rsRetVal DoSaveOnShutdown(qqueue_t *pThis);

/* do cleanup when config is loaded */
void qqueueDoneLoadCnf(void) {
    struct queue_filename *next, *del;
    next = queue_filename_root;
    while (next != NULL) {
        del = next;
        next = next->next;
        free((void *)del->filename);
        free((void *)del->dirname);
        free((void *)del);
    }
    queue_filename_root = NULL;
}


/***********************************************************************
 * we need a private data structure, the "to-delete" list. As C does
 * not provide any partly private data structures, we implement this
 * structure right here inside the module.
 * Note that this list must always be kept sorted based on a unique
 * dequeue ID (which is monotonically increasing).
 * rgerhards, 2009-05-18
 ***********************************************************************/

/* generate next uniqueue dequeue ID. Note that uniqueness is only required
 * on a per-queue basis and while this instance runs. So a stricly monotonically
 * increasing counter is sufficient (if enough bits are used).
 */
static inline qDeqID getNextDeqID(qqueue_t *pQueue) {
    ISOBJ_TYPE_assert(pQueue, qqueue);
    return pQueue->deqIDAdd++;
}


/* return the top element of the to-delete list or NULL, if the
 * list is empty.
 */
static toDeleteLst_t *tdlPeek(qqueue_t *pQueue) {
    ISOBJ_TYPE_assert(pQueue, qqueue);
    return pQueue->toDeleteLst;
}


/* remove the top element of the to-delete list. Nothing but the
 * element itself is destroyed. Must not be called when the list
 * is empty.
 */
static rsRetVal tdlPop(qqueue_t *pQueue) {
    toDeleteLst_t *pRemove;
    DEFiRet;

    ISOBJ_TYPE_assert(pQueue, qqueue);
    assert(pQueue->toDeleteLst != NULL);

    pRemove = pQueue->toDeleteLst;
    pQueue->toDeleteLst = pQueue->toDeleteLst->pNext;
    free(pRemove);

    RETiRet;
}


/* Add a new to-delete list entry. The function allocates the data
 * structure, populates it with the values provided and links the new
 * element into the correct place inside the list.
 */
static rsRetVal tdlAdd(qqueue_t *pQueue, qDeqID deqID, int nElemDeq) {
    toDeleteLst_t *pNew;
    toDeleteLst_t *pPrev;
    toDeleteLst_t *pCur;
    DEFiRet;

    ISOBJ_TYPE_assert(pQueue, qqueue);
    assert(pQueue->toDeleteLst != NULL);

    CHKmalloc(pNew = malloc(sizeof(toDeleteLst_t)));
    pNew->deqID = deqID;
    pNew->nElemDeq = nElemDeq;

    /* now find right spot */
    pPrev = NULL;
    for (pCur = pQueue->toDeleteLst; pCur != NULL && deqID > pCur->deqID; pCur = pCur->pNext) {
        pPrev = pCur;
        /*JUST SEARCH*/;
    }

    if (pPrev == NULL) {
        pNew->pNext = pCur;
        pQueue->toDeleteLst = pNew;
    } else {
        pNew->pNext = pCur;
        pPrev->pNext = pNew;
    }

finalize_it:
    RETiRet;
}


static rsRetVal validateQueueSpoolDir(qqueue_t *pThis) {
    DEFiRet;

    if (pThis->pszSpoolDir != NULL && pThis->lenSpoolDir == 0) {
        parser_errmsg("queue.spooldirectory must not be empty");
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }

finalize_it:
    RETiRet;
}


/* methods */

static const char *getQueueTypeName(queueType_t t) {
    const char *r;

    switch (t) {
        case QUEUETYPE_FIXED_ARRAY:
            r = "FixedArray";
            break;
        case QUEUETYPE_LINKEDLIST:
            r = "LinkedList";
            break;
        case QUEUETYPE_DISK:
            r = "Disk";
            break;
        case QUEUETYPE_DIRECT:
            r = "Direct";
            break;
        case QUEUETYPE_SEGMENTED_DISK:
            r = "segmentedDisk";
            break;
        default:
            r = "invalid/unknown queue mode";
            break;
    }
    return r;
}

void qqueueDbgPrint(qqueue_t *pThis) {
    dbgoprint((obj_t *)pThis, "parameter dump:\n");
    dbgoprint((obj_t *)pThis, "queue.filename '%s'\n",
              (pThis->pszFilePrefix == NULL) ? "[NONE]" : (char *)pThis->pszFilePrefix);
    dbgoprint((obj_t *)pThis, "queue.size: %d\n", pThis->iMaxQueueSize);
    dbgoprint((obj_t *)pThis, "queue.dequeuebatchsize: %d\n", pThis->iDeqBatchSize);
    dbgoprint((obj_t *)pThis, "queue.mutexcontentionstats: %d\n", pThis->bMutexContentionStats);
    dbgoprint((obj_t *)pThis, "queue.mindequeuebatchsize: %d\n", pThis->iMinDeqBatchSize);
    dbgoprint((obj_t *)pThis, "queue.mindequeuebatchsize.timeout: %d\n", pThis->toMinDeqBatchSize);
    dbgoprint((obj_t *)pThis, "queue.maxdiskspace: %lld\n", pThis->sizeOnDiskMax);
    dbgoprint((obj_t *)pThis, "queue.highwatermark: %d\n", pThis->iHighWtrMrk);
    dbgoprint((obj_t *)pThis, "queue.lowwatermark: %d\n", pThis->iLowWtrMrk);
    dbgoprint((obj_t *)pThis, "queue.fulldelaymark: %d\n", pThis->iFullDlyMrk);
    dbgoprint((obj_t *)pThis, "queue.lightdelaymark: %d\n", pThis->iLightDlyMrk);
    dbgoprint((obj_t *)pThis, "queue.takeflowctlfrommsg: %d\n", pThis->takeFlowCtlFromMsg);
    dbgoprint((obj_t *)pThis, "queue.discardmark: %d\n", pThis->iDiscardMrk);
    dbgoprint((obj_t *)pThis, "queue.discardseverity: %d\n", pThis->iDiscardSeverity);
    dbgoprint((obj_t *)pThis, "queue.checkpointinterval: %d\n", pThis->iPersistUpdCnt);
    dbgoprint((obj_t *)pThis, "queue.syncqueuefiles: %d\n", pThis->bSyncQueueFiles);
    dbgoprint((obj_t *)pThis, "queue.type: %d [%s]\n", pThis->qType, getQueueTypeName(pThis->qType));
    dbgoprint((obj_t *)pThis, "queue.workerthreads: %d\n", pThis->iNumWorkerThreads);
    dbgoprint((obj_t *)pThis, "queue.timeoutshutdown: %d\n", pThis->toQShutdown);
    dbgoprint((obj_t *)pThis, "queue.timeoutactioncompletion: %d\n", pThis->toActShutdown);
    dbgoprint((obj_t *)pThis, "queue.timeoutenqueue: %d\n", pThis->toEnq);
    dbgoprint((obj_t *)pThis, "queue.timeoutworkerthreadshutdown: %d\n", pThis->toWrkShutdown);
    dbgoprint((obj_t *)pThis, "queue.workerthreadminimummessages: %d\n", pThis->iMinMsgsPerWrkr);
    dbgoprint((obj_t *)pThis, "queue.maxfilesize: %lld\n", pThis->iMaxFileSize);
    dbgoprint((obj_t *)pThis, "queue.saveonshutdown: %d\n", pThis->bSaveOnShutdown);
    dbgoprint((obj_t *)pThis, "queue.dequeueslowdown: %d\n", pThis->iDeqSlowdown);
    dbgoprint((obj_t *)pThis, "queue.dequeuetimebegin: %d\n", pThis->iDeqtWinFromHr);
    dbgoprint((obj_t *)pThis, "queue.dequeuetimeend: %d\n", pThis->iDeqtWinToHr);
}


/* get the physical queue size. Must only be called
 * while mutex is locked!
 * rgerhards, 2008-01-29
 */
static int getPhysicalQueueSize(qqueue_t *pThis) {
    const int size = (int)PREFER_FETCH_32BIT(pThis->iQueueSize);
    if (size == 0 && pThis->qType == QUEUETYPE_SEGMENTED_DISK && pThis->tVars.segdisk != NULL &&
        segdiskStoreMayHaveData(pThis->tVars.segdisk))
        return 1;
    return size;
}


/* get the logical queue size (that is store size minus logically dequeued elements).
 * Must only be called while mutex is locked!
 * rgerhards, 2009-05-19
 */
static int getLogicalQueueSize(qqueue_t *pThis) {
    const int size = pThis->iQueueSize - pThis->nLogDeq;
    if (size == 0 && pThis->qType == QUEUETYPE_SEGMENTED_DISK && pThis->tVars.segdisk != NULL &&
        segdiskStoreMayHaveData(pThis->tVars.segdisk))
        return 1;
    return size;
}

/*
 * Acquire the queue mutex while optionally recording observable contention.
 * The normal path deliberately remains a plain mutex lock. The diagnostic
 * path is selected explicitly per queue because trylock affects throughput.
 * Observed wait time includes scheduler delay and is not mutex hold time.
 */
static void qqueueLock(qqueue_t *const pThis) {
    if (!pThis->bMutexContentionStats || !STATSCOUNTER_ENABLED()) {
        d_pthread_mutex_lock(pThis->mut);
        return;
    }

    const int trylock_ret = d_pthread_mutex_trylock(pThis->mut);
    if (trylock_ret == 0) return;
    if (trylock_ret != EBUSY) {
        d_pthread_mutex_lock(pThis->mut);
        return;
    }

    STATSCOUNTER_INC(pThis->ctrMutexContention, pThis->mutCtrMutexContention);
    struct timespec before;
    const int have_before = clock_gettime(CLOCK_MONOTONIC, &before) == 0;
    d_pthread_mutex_lock(pThis->mut);
    if (have_before) {
        struct timespec after;
        if (clock_gettime(CLOCK_MONOTONIC, &after) == 0) {
            const int64_t wait_ns =
                ((int64_t)(after.tv_sec - before.tv_sec) * 1000000000LL) + (int64_t)(after.tv_nsec - before.tv_nsec);
            if (wait_ns >= 0) STATSCOUNTER_ADD(pThis->ctrMutexWaitNs, pThis->mutCtrMutexWaitNs, (uint64_t)wait_ns);
        }
    }
}

static int64 getQueueDiskBytes(qqueue_t *pThis) {
    if (pThis->qType == QUEUETYPE_SEGMENTED_DISK && pThis->tVars.segdisk != NULL) {
        segdisk_store_stats_t stats;
        segdiskStoreGetStats(pThis->tVars.segdisk, &stats);
        return stats.bytes;
    }
    return pThis->tVars.disk.sizeOnDisk;
}

static int segdiskStatsInt(const uint64_t value) {
    return value > INT_MAX ? INT_MAX : (int)value;
}

static void qqueueUpdateSegDiskStats(qqueue_t *pThis) {
    if (pThis->qType != QUEUETYPE_SEGMENTED_DISK || pThis->tVars.segdisk == NULL) return;
    segdisk_store_stats_t stats;
    segdiskStoreGetStats(pThis->tVars.segdisk, &stats);
    /* Writers remain serialized by the queue mutex; impstats reads these
     * published integer mirrors without that mutex via PREFER_LOAD_INT. */
    PREFER_STORE_INT(&pThis->segdiskBytes, stats.bytes > INT_MAX ? INT_MAX : (stats.bytes < 0 ? 0 : (int)stats.bytes));
    PREFER_STORE_INT(&pThis->segdiskSegments, stats.segments);
    PREFER_STORE_INT(&pThis->segdiskCheckpoints, segdiskStatsInt(stats.checkpoints));
    PREFER_STORE_INT(&pThis->segdiskReplayed, segdiskStatsInt(stats.replayed));
    PREFER_STORE_INT(&pThis->segdiskCorruptionEvents, segdiskStatsInt(stats.corruption_events));
    PREFER_STORE_INT(&pThis->segdiskCorruptionBytes, segdiskStatsInt(stats.corruption_bytes));
    PREFER_STORE_INT(&pThis->segdiskCorruptionRecords, segdiskStatsInt(stats.corruption_records));
    PREFER_STORE_INT(&pThis->segdiskRetryOverageBytes,
                     stats.retry_overage_bytes > INT_MAX ? INT_MAX : (int)stats.retry_overage_bytes);
    PREFER_STORE_INT(&pThis->segdiskRetryOverageMaxBytes,
                     stats.retry_overage_max_bytes > INT_MAX ? INT_MAX : (int)stats.retry_overage_max_bytes);
    PREFER_STORE_INT(&pThis->segdiskStateWrites, segdiskStatsInt(stats.state_writes));
    PREFER_STORE_INT(&pThis->segdiskForcedStateWrites, segdiskStatsInt(stats.forced_state_writes));
    PREFER_STORE_INT(&pThis->segdiskRecoveryBytes, segdiskStatsInt(stats.recovery_bytes));
    PREFER_STORE_INT(&pThis->segdiskRecoveryRecords, segdiskStatsInt(stats.recovery_records));
    PREFER_STORE_INT(&pThis->segdiskStartupPayloadBytes, segdiskStatsInt(stats.startup_payload_bytes_read));
    PREFER_STORE_INT(&pThis->segdiskStartupSegmentFilesProbed, segdiskStatsInt(stats.startup_segment_files_probed));
    PREFER_STORE_INT(&pThis->segdiskRecoveryPending, segdiskStatsInt(stats.recovery_pending));
    PREFER_STORE_INT(&pThis->segdiskCorruptionSegments, segdiskStatsInt(stats.corruption_segments));
    PREFER_STORE_INT(&pThis->segdiskMaterializations, segdiskStatsInt(stats.materializations));
    PREFER_STORE_INT(&pThis->segdiskDematerializations, segdiskStatsInt(stats.dematerializations));
    PREFER_STORE_INT(&pThis->segdiskIdleCleanupFailures, segdiskStatsInt(stats.idle_cleanup_failures));
}

static rsRetVal qqueueSegDiskIdleTimeout(qqueue_t *pThis) {
    if (!pThis->segdiskDAChild || pThis->qType != QUEUETYPE_SEGMENTED_DISK || pThis->tVars.segdisk == NULL)
        return RS_RET_RETRY;
    if (getLogicalQueueSize(pThis) != 0 || getPhysicalQueueSize(pThis) != 0 ||
        !segdiskStoreCanDematerialize(pThis->tVars.segdisk))
        return RS_RET_RETRY;
    /* A DA child shares its parent queue mutex.  The child can become empty
     * while ConsumerDA still owns a parent batch that has not reached the
     * child store.  The parent physical count includes those logically
     * dequeued batches until ConsumerDA commits them, so it is the required
     * transfer-in-flight barrier.  Without it, an immediate idle timeout can
     * remove the child store and a crash can lose the parent-only batch. */
    if (pThis->pqParent == NULL || getPhysicalQueueSize(pThis->pqParent) != 0) {
        return RS_RET_RETRY;
    }
    if (pThis->segdiskIdleObservedActivity != pThis->pqParent->daActivityGeneration) {
        pThis->segdiskIdleObservedActivity = pThis->pqParent->daActivityGeneration;
        return RS_RET_RETRY;
    }
    const rsRetVal r = segdiskStoreDematerialize(pThis->tVars.segdisk);
    qqueueUpdateSegDiskStats(pThis);
    if (r != RS_RET_OK) {
        LogError(0, r, "%s: segmented disk-assisted idle store cleanup failed; will retry",
                 obj.GetName((obj_t *)pThis));
        return RS_RET_RETRY;
    }
    return RS_RET_OK;
}

#ifdef ENABLE_IMDIAG
rsRetVal qqueueSetSegDiskTestFault(qqueue_t *pThis, const char *point, unsigned int hit_count) {
    if (pThis == NULL || point == NULL || hit_count == 0) return RS_RET_PARAM_ERROR;
    qqueue_t *target = pThis->qType == QUEUETYPE_SEGMENTED_DISK ? pThis : pThis->pqDA;
    if (target == NULL || target->qType != QUEUETYPE_SEGMENTED_DISK || target->tVars.segdisk == NULL)
        return RS_RET_INVALID_VALUE;
    d_pthread_mutex_lock(target->mut);
    const rsRetVal r = segdiskStoreSetTestFault(target->tVars.segdisk, point, hit_count);
    d_pthread_mutex_unlock(target->mut);
    return r;
}

rsRetVal qqueueClearSegDiskTestFault(qqueue_t *pThis) {
    if (pThis == NULL) return RS_RET_PARAM_ERROR;
    qqueue_t *target = pThis->qType == QUEUETYPE_SEGMENTED_DISK ? pThis : pThis->pqDA;
    if (target == NULL || target->qType != QUEUETYPE_SEGMENTED_DISK || target->tVars.segdisk == NULL)
        return RS_RET_INVALID_VALUE;
    d_pthread_mutex_lock(target->mut);
    segdiskStoreClearTestFault(target->tVars.segdisk);
    d_pthread_mutex_unlock(target->mut);
    return RS_RET_OK;
}
#endif


static int qqueueIsShutdownImmediate(qqueue_t *const pThis) {
    return ATOMIC_LOAD_32BIT(&pThis->bShutdownImmediate, &pThis->mutShutdownImmediate);
}


static void qqueueSetShutdownImmediate(qqueue_t *const pThis, const int value) {
    ATOMIC_STORE_32BIT(&pThis->bShutdownImmediate, &pThis->mutShutdownImmediate, value);
}


static void qqueueSetWtiShutdownImmediate(qqueue_t *const pThis, wti_t *const pWti) {
    pWti->pbShutdownImmediate = &pThis->bShutdownImmediate;
#ifndef HAVE_ATOMIC_BUILTINS
    pWti->pmutShutdownImmediate = &pThis->mutShutdownImmediate;
#endif
}

/* The worker framework holds pWtp->pmutUsr while it invokes queue callbacks.
 * A shared mutex alone is not permission to service another queue: S1 only
 * records batches acquired from the pool's own user queue. */
static rsRetVal qqueueBindWtiSource(qqueue_t *const pThis, wti_t *const pWti) {
    return qqueueLeaseBind(
        &pWti->source_queue, &pWti->logical_owner, pThis, pThis->pqParent == NULL ? pThis : pThis->pqParent,
        pWti->pWtp == NULL ? NULL : pWti->pWtp->pUsr, pWti->pWtp == NULL ? NULL : pWti->pWtp->pmutUsr, pThis->mut);
}

static rsRetVal qqueueClearWtiSource(qqueue_t *const pThis, wti_t *const pWti) {
    return qqueueLeaseClear(&pWti->source_queue, &pWti->logical_owner, pThis);
}


/* This function drains the queue in cases where this needs to be done. The most probable
 * reason is a HUP which needs to discard data (because the queue is configured to be lossy).
 * During a shutdown, this is typically not needed, as the OS frees up resources and does
 * this much quicker than when we clean up ourselves. -- rgerhards, 2008-10-21
 * This function returns void, as it makes no sense to communicate an error back, even if
 * it happens.
 * This functions works "around" the regular deque mechanism, because it is only used to
 * clean up (in cases where message loss is acceptable).
 */
static void queueDrain(qqueue_t *pThis) {
    smsg_t *pMsg;
    assert(pThis != NULL);

    DBGOPRINT((obj_t *)pThis, "queue (type %d) will lose %d messages, destroying...\n", pThis->qType,
              pThis->iQueueSize);
    /* iQueueSize is not decremented by qDel(), so we need to do it ourselves */
    while (ATOMIC_DEC_AND_FETCH(&pThis->iQueueSize, &pThis->mutQueueSize) > 0) {
        pThis->qDeq(pThis, &pMsg);
        if (pMsg != NULL) {
            msgDestruct(&pMsg);
        }
        pThis->qDel(pThis);
    }
}


/* --------------- code for disk-assisted (DA) queue modes -------------------- */


/* returns the number of workers that should be advised at
 * this point in time. The mutex must be locked when
 * ths function is called. -- rgerhards, 2008-01-25
 */
static rsRetVal qqueueAdviseMaxWorkers(qqueue_t *pThis) {
    DEFiRet;
    int iMaxWorkers;

    ISOBJ_TYPE_assert(pThis, qqueue);

    if (pThis->localGraphConf != NULL && !pThis->localGraphReady) return RS_RET_OK;
    if (!pThis->bEnqOnly) {
        if (pThis->bIsDA && getLogicalQueueSize(pThis) >= pThis->iHighWtrMrk) {
            DBGOPRINT((obj_t *)pThis, "(re)activating DA worker\n");
            pThis->localDAActive = 1;
            wtpAdviseMaxWorkers(pThis->pWtpDA, 1, DENY_WORKER_START_DURING_SHUTDOWN);
            /* The DA transfer pool intentionally has one worker. */
        }
        if (getLogicalQueueSize(pThis) == 0) {
            iMaxWorkers = 0;
        } else if (pThis->iMinMsgsPerWrkr == 0) {
            iMaxWorkers = 1;
        } else {
            iMaxWorkers = getLogicalQueueSize(pThis) / pThis->iMinMsgsPerWrkr + 1;
        }
        wtpAdviseMaxWorkers(pThis->pWtpReg, iMaxWorkers, DENY_WORKER_START_DURING_SHUTDOWN);
    }

    RETiRet;
}


/* check if we run in disk-assisted mode and record that
 * setting for easy (and quick!) access in the future. This
 * function must only be called from constructors and only
 * from those that support disk-assisted modes (aka memory-
 * based queue drivers).
 * rgerhards, 2008-01-14
 */
static rsRetVal qqueueChkIsDA(qqueue_t *pThis) {
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    if (pThis->pszFilePrefix != NULL) {
        pThis->bIsDA = 1;
        DBGOPRINT((obj_t *)pThis, "is disk-assisted, disk will be used on demand\n");
    } else {
        DBGOPRINT((obj_t *)pThis, "is NOT disk-assisted\n");
    }

    RETiRet;
}


/**
 * @brief Construct and configure the persistent child of a memory-first DA queue.
 *
 * Disk-assisted queues have an in-memory parent and a separate child that
 * receives overflow. The resolver chooses the child's concrete store before
 * construction, preserving legacy backlogs and rejecting mixed stores rather
 * than attempting in-place conversion. For a fresh segmented child, this
 * routine deliberately leaves the store unmaterialized: the marker, .segq
 * directory, state file, and disk worker are created only on the first spill.
 *
 * The durable marker is published before opening existing data or replacing a
 * drained classic selection. A fresh child instead carries marker-pending
 * state until its first append, which keeps normal memory-only operation free
 * of disk artifacts while still preventing a later engine ambiguity.
 */
static rsRetVal StartDA(qqueue_t *pThis) {
    DEFiRet;
    uchar pszDAQName[128];

    ISOBJ_TYPE_assert(pThis, qqueue);

    const qda_engine_config_t engine_config = {
        .spool_dir = (const char *)pThis->pszSpoolDir,
        .file_prefix = (const char *)pThis->pszFilePrefix,
        .requested = pThis->diskQueueType,
        .auto_upgrade = pThis->diskQueueAutoUpgrade,
        .requires_classic_features = pThis->useCryprov || pThis->onCorruption != QUEUE_ON_CORRUPTION_SAFE_MODE,
    };
    qda_engine_result_t engine_result;
    iRet = qdaEngineResolve(&engine_config, &engine_result);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet,
                 "%s: cannot select disk-assisted queue engine for prefix '%s'; classic and segmented data, "
                 "an explicit engine conflict, a malformed engine marker, or unsupported segmented options "
                 "require operator intervention",
                 obj.GetName((obj_t *)pThis), pThis->pszFilePrefix);
        FINALIZE;
    }
    pThis->effectiveDiskQueueType = engine_result.effective;
    if (pThis->diskQueueType == QDA_ENGINE_AUTO && engine_result.effective == QDA_ENGINE_DISK) {
        if (engine_result.reason == QDA_REASON_CLASSIC_FEATURE)
            LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                   "%s: disk-assisted queue automatically selected the classic disk engine for prefix '%s' "
                   "because classic-only queue options are configured",
                   obj.GetName((obj_t *)pThis), pThis->pszFilePrefix);
        else
            LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                   "%s: disk-assisted queue automatically selected the classic disk engine for prefix '%s' "
                   "because classic queue state is present or retained; set queue.diskQueueAutoUpgrade=\"on\" "
                   "to use segmentedDisk after the classic backlog drains",
                   obj.GetName((obj_t *)pThis), pThis->pszFilePrefix);
    }
    /* A marker mismatch is the intentional auto-upgrade case.  The resolver
     * permits it only after proving that the old classic store is empty, so
     * publish the new selection durably at this restart boundary. */
    if (engine_result.classic_data || engine_result.segmented_data ||
        (engine_result.marker_present && engine_result.marker_engine != engine_result.effective)) {
        CHKiRet(qdaEngineWriteMarker((const char *)pThis->pszSpoolDir, (const char *)pThis->pszFilePrefix,
                                     engine_result.effective));
    }

    const queueType_t child_type =
        engine_result.effective == QDA_ENGINE_SEGMENTED_DISK ? QUEUETYPE_SEGMENTED_DISK : QUEUETYPE_DISK;
    /* create message queue */
    CHKiRet(qqueueConstruct(&pThis->pqDA, child_type, pThis->iNumWorkerThreads, 0, pThis->pConsumer));

    /* give it a name */
    snprintf((char *)pszDAQName, sizeof(pszDAQName), "%s[DA]", obj.GetName((obj_t *)pThis));
    obj.SetName((obj_t *)pThis->pqDA, pszDAQName);

    /* as the created queue is the same object class, we take the
     * liberty to access its properties directly.
     */
    pThis->pqDA->pqParent = pThis;
    pThis->pqDA->localGraphConf = pThis->localGraphConf;
    pThis->pqDA->localGraphReady = pThis->localGraphReady;
    pThis->pqDA->segdiskDAChild = child_type == QUEUETYPE_SEGMENTED_DISK;
    pThis->pqDA->segdiskLazyCreate = pThis->pqDA->segdiskDAChild && !engine_result.segmented_data;
    pThis->pqDA->daEngineMarkerPending =
        !engine_result.marker_present && !engine_result.classic_data && !engine_result.segmented_data;
    pThis->pqDA->diskQueueIdleTimeout = pThis->diskQueueIdleTimeout;

    CHKiRet(qqueueSetpAction(pThis->pqDA, pThis->pAction));
    CHKiRet(qqueueSetsizeOnDiskMax(pThis->pqDA, pThis->sizeOnDiskMax));
    CHKiRet(qqueueSetiDeqSlowdown(pThis->pqDA, pThis->iDeqSlowdown));
    CHKiRet(qqueueSetMaxFileSize(pThis->pqDA, pThis->iMaxFileSize));
    CHKiRet(qqueueSetFilePrefix(pThis->pqDA, pThis->pszFilePrefix, pThis->lenFilePrefix));
    CHKiRet(qqueueSetSpoolDir(pThis->pqDA, pThis->pszSpoolDir, pThis->lenSpoolDir));
    CHKiRet(qqueueSetiPersistUpdCnt(pThis->pqDA, pThis->iPersistUpdCnt));
    CHKiRet(qqueueSetbSyncQueueFiles(pThis->pqDA, pThis->bSyncQueueFiles));
    CHKiRet(qqueueSettoActShutdown(pThis->pqDA, pThis->toActShutdown));
    CHKiRet(qqueueSettoEnq(pThis->pqDA, pThis->toEnq));
    CHKiRet(qqueueSetiDeqtWinFromHr(pThis->pqDA, pThis->iDeqtWinFromHr));
    CHKiRet(qqueueSetiDeqtWinToHr(pThis->pqDA, pThis->iDeqtWinToHr));
    CHKiRet(qqueueSettoQShutdown(pThis->pqDA, pThis->toQShutdown));
    CHKiRet(qqueueSetiHighWtrMrk(pThis->pqDA, 0));
    CHKiRet(qqueueSetiDiscardMrk(pThis->pqDA, 0));
    pThis->pqDA->iDeqBatchSize = pThis->iDeqBatchSize;
    pThis->pqDA->iMinDeqBatchSize = pThis->iMinDeqBatchSize;
    pThis->pqDA->iMinMsgsPerWrkr = pThis->iMinMsgsPerWrkr;
    pThis->pqDA->iLowWtrMrk = pThis->iLowWtrMrk;
    pThis->pqDA->onCorruption = pThis->onCorruption;
    if (pThis->useCryprov && child_type == QUEUETYPE_DISK) {
        /* hand over cryprov to DA queue - in-mem queue does no longer need it
         * and DA queue will be kept active from now on until termination.
         */
        pThis->pqDA->useCryprov = pThis->useCryprov;
        pThis->pqDA->cryprov = pThis->cryprov;
        pThis->pqDA->cryprovData = pThis->cryprovData;
        pThis->pqDA->cryprovName = pThis->cryprovName;
        pThis->pqDA->cryprovNameFull = pThis->cryprovNameFull;
        /* reset memory queue parameters */
        pThis->useCryprov = 0;
        /* pThis->cryprov cannot and need not be reset, is structure */
        pThis->cryprovData = NULL;
        pThis->cryprovName = NULL;
        pThis->cryprovNameFull = NULL;
    }

    iRet = qqueueStart(runConf, pThis->pqDA);
    /* file not found is expected, that means it is no previous QIF available */
    if (iRet != RS_RET_OK && iRet != RS_RET_FILE_NOT_FOUND) {
        errno = 0; /* else an errno is shown in errmsg! */
        LogError(errno, iRet, "error starting up disk queue, using pure in-memory mode");
        pThis->bIsDA = 0; /* disable memory mode */
        FINALIZE; /* something is wrong */
    }

    DBGOPRINT((obj_t *)pThis, "DA queue initialized, disk queue 0x%lx\n", qqueueGetID(pThis->pqDA));

finalize_it:
    if (iRet != RS_RET_OK) {
        const rsRetVal startup_error = iRet;
        if (pThis->pqDA != NULL) {
            qqueueDestruct(&pThis->pqDA);
        }
        pThis->bIsDA = 0;
        LogError(0, startup_error, "%s: error creating disk queue - giving up.", obj.GetName((obj_t *)pThis));
    }

    RETiRet;
}


/* initiate DA mode
 * param bEnqOnly tells if the disk queue is to be run in enqueue-only mode. This may
 * be needed during shutdown of memory queues which need to be persisted to disk.
 * If this function fails (should not happen), DA mode is not turned on.
 * rgerhards, 2008-01-16
 */
static rsRetVal ATTR_NONNULL() InitDA(qqueue_t *const pThis, const int bLockMutex) {
    DEFiRet;
    uchar pszBuf[64];
    size_t lenBuf;

    ISOBJ_TYPE_assert(pThis, qqueue);
    if (bLockMutex == LOCK_MUTEX) {
        d_pthread_mutex_lock(pThis->mut);
    }

    /* check if we already have a DA worker pool. If not, initiate one. Please note that the
     * pool is created on first need but never again destructed (until the queue is). This
     * is intentional. We assume that when we need it once, we may also need it on another
     * occasion. Resources used are quite minimal when no worker is running.
     * rgerhards, 2008-01-24
     * NOTE: this is the DA worker *pool*, not the DA queue!
     */
    lenBuf = snprintf((char *)pszBuf, sizeof(pszBuf), "%s:DAwpool", obj.GetName((obj_t *)pThis));
    CHKiRet(wtpConstruct(&pThis->pWtpDA));
    CHKiRet(wtpSetDbgHdr(pThis->pWtpDA, pszBuf, lenBuf));
    CHKiRet(wtpSetpfChkStopWrkr(pThis->pWtpDA, (rsRetVal(*)(void *pUsr, int))qqueueChkStopWrkrDA));
    CHKiRet(wtpSetpfGetDeqBatchSize(pThis->pWtpDA, (rsRetVal(*)(void *pUsr, int *))GetDeqBatchSize));
    CHKiRet(wtpSetpfDoWork(pThis->pWtpDA, (rsRetVal(*)(void *pUsr, void *pWti))ConsumerDA));
    CHKiRet(wtpSetpfObjProcessed(pThis->pWtpDA, (rsRetVal(*)(void *pUsr, wti_t *pWti))batchProcessed));
    CHKiRet(wtpSetpmutUsr(pThis->pWtpDA, pThis->mut));
    CHKiRet(wtpSetiNumWorkerThreads(pThis->pWtpDA, 1));
    CHKiRet(wtpSettoWrkShutdown(pThis->pWtpDA, pThis->toWrkShutdown));
    CHKiRet(wtpSetpUsr(pThis->pWtpDA, pThis));
    if (pThis->localGraphConf != NULL) CHKiRet(wtpUseMonotonicTermination(pThis->pWtpDA));
    CHKiRet(wtpConstructFinalize(pThis->pWtpDA));
    /* if we reach this point, we have a "good" DA worker pool */

    /* now construct the actual queue (if it does not already exist) */
    if (pThis->pqDA == NULL) {
        CHKiRet(StartDA(pThis));
    }

finalize_it:
    if (iRet != RS_RET_OK) pThis->bIsDA = 0;
    if (bLockMutex == LOCK_MUTEX) {
        d_pthread_mutex_unlock(pThis->mut);
    }
    RETiRet;
}


/* --------------- end code for disk-assisted queue modes -------------------- */


/* Now, we define type-specific handlers. The provide a generic functionality,
 * but for this specific type of queue. The mapping to these handlers happens during
 * queue construction. Later on, handlers are called by pointers present in the
 * queue instance object.
 */

/* -------------------- fixed array -------------------- */
static rsRetVal qConstructFixedArray(qqueue_t *pThis) {
    DEFiRet;

    assert(pThis != NULL);

    if (pThis->iMaxQueueSize == 0) ABORT_FINALIZE(RS_RET_QSIZE_ZERO);

    if ((pThis->tVars.farray.pBuf = malloc(sizeof(void *) * pThis->iMaxQueueSize)) == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    pThis->tVars.farray.deqhead = 0;
    pThis->tVars.farray.head = 0;
    pThis->tVars.farray.tail = 0;

    qqueueChkIsDA(pThis);

finalize_it:
    RETiRet;
}


static rsRetVal qDestructFixedArray(qqueue_t *pThis) {
    DEFiRet;

    assert(pThis != NULL);

    if (pThis->bLocalScope) {
        /* Local family shutdown already reconciled every retained reference. */
        assert(pThis->iQueueSize == 0);
    } else {
        queueDrain(pThis); /* discard any remaining queue entries */
    }
    free(pThis->tVars.farray.pBuf);

    RETiRet;
}


static rsRetVal qAddFixedArray(qqueue_t *pThis, smsg_t *in) {
    DEFiRet;

    assert(pThis != NULL);
    pThis->tVars.farray.pBuf[pThis->tVars.farray.tail] = in;
    pThis->tVars.farray.tail++;
    if (pThis->tVars.farray.tail == pThis->iMaxQueueSize) pThis->tVars.farray.tail = 0;

    RETiRet;
}


static rsRetVal qDeqFixedArray(qqueue_t *pThis, smsg_t **out) {
    DEFiRet;

    assert(pThis != NULL);
    *out = (void *)pThis->tVars.farray.pBuf[pThis->tVars.farray.deqhead];

    pThis->tVars.farray.deqhead++;
    if (pThis->tVars.farray.deqhead == pThis->iMaxQueueSize) pThis->tVars.farray.deqhead = 0;

    RETiRet;
}


static rsRetVal qDelFixedArray(qqueue_t *pThis) {
    DEFiRet;

    assert(pThis != NULL);

    pThis->tVars.farray.head++;
    if (pThis->tVars.farray.head == pThis->iMaxQueueSize) pThis->tVars.farray.head = 0;

    RETiRet;
}


/* -------------------- linked list  -------------------- */


static rsRetVal qConstructLinkedList(qqueue_t *pThis) {
    DEFiRet;

    assert(pThis != NULL);

    pThis->tVars.linklist.pDeqRoot = NULL;
    pThis->tVars.linklist.pDelRoot = NULL;
    pThis->tVars.linklist.pLast = NULL;

    qqueueChkIsDA(pThis);

    RETiRet;
}


static rsRetVal qDestructLinkedList(qqueue_t __attribute__((unused)) * pThis) {
    DEFiRet;

    queueDrain(pThis); /* discard any remaining queue entries */

    /* with the linked list type, there is nothing left to do here. The
     * reason is that there are no dynamic elements for the list itself.
     */

    RETiRet;
}

static rsRetVal qAddLinkedList(qqueue_t *pThis, smsg_t *pMsg) {
    qLinkedList_t *pEntry;
    DEFiRet;

    CHKmalloc((pEntry = (qLinkedList_t *)malloc(sizeof(qLinkedList_t))));

    pEntry->pNext = NULL;
    pEntry->pMsg = pMsg;

    if (pThis->tVars.linklist.pDelRoot == NULL) {
        pThis->tVars.linklist.pDelRoot = pThis->tVars.linklist.pDeqRoot = pThis->tVars.linklist.pLast = pEntry;
    } else {
        pThis->tVars.linklist.pLast->pNext = pEntry;
        pThis->tVars.linklist.pLast = pEntry;
    }

    if (pThis->tVars.linklist.pDeqRoot == NULL) {
        pThis->tVars.linklist.pDeqRoot = pEntry;
    }

finalize_it:
    RETiRet;
}


static rsRetVal qDeqLinkedList(qqueue_t *pThis, smsg_t **ppMsg) {
    qLinkedList_t *pEntry;
    DEFiRet;

    pEntry = pThis->tVars.linklist.pDeqRoot;
    if (pEntry != NULL) {
        *ppMsg = pEntry->pMsg;
        pThis->tVars.linklist.pDeqRoot = pEntry->pNext;
    } else {
        /* Check and return NULL for linklist.pDeqRoot */
        dbgprintf("qDeqLinkedList: pDeqRoot is NULL!\n");
        *ppMsg = NULL;
        pThis->tVars.linklist.pDeqRoot = NULL;
    }

    RETiRet;
}


static rsRetVal qDelLinkedList(qqueue_t *pThis) {
    qLinkedList_t *pEntry;
    DEFiRet;

    pEntry = pThis->tVars.linklist.pDelRoot;

    if (pThis->tVars.linklist.pDelRoot == pThis->tVars.linklist.pLast) {
        pThis->tVars.linklist.pDelRoot = pThis->tVars.linklist.pDeqRoot = pThis->tVars.linklist.pLast = NULL;
    } else {
        pThis->tVars.linklist.pDelRoot = pEntry->pNext;
    }

    free(pEntry);

    RETiRet;
}


/* -------------------- disk  -------------------- */


/* Helper to switch queue to LinkedList mode */
static void qqueueSetupLinkedList(qqueue_t *pThis) {
    pThis->qConstruct = qConstructLinkedList;
    pThis->qDestruct = qDestructLinkedList;
    pThis->qAdd = qAddLinkedList;
    pThis->qDeq = qDeqLinkedList;
    pThis->qDel = qDelLinkedList;
    pThis->MultiEnq = qqueueMultiEnqObjNonDirect;
}

static void qqueueDestroyDiskStreams(qqueue_t *pThis) {
    /* Emergency recovery must not delete queue files while switching modes. */
    if (pThis->tVars.disk.pWrite != NULL) strm.SetbDeleteOnClose(pThis->tVars.disk.pWrite, 0);
    if (pThis->tVars.disk.pReadDeq != NULL) strm.SetbDeleteOnClose(pThis->tVars.disk.pReadDeq, 0);
    if (pThis->tVars.disk.pReadDel != NULL) strm.SetbDeleteOnClose(pThis->tVars.disk.pReadDel, 0);
    if (pThis->tVars.disk.pWrite != NULL) strm.Destruct(&pThis->tVars.disk.pWrite);
    if (pThis->tVars.disk.pReadDeq != NULL) strm.Destruct(&pThis->tVars.disk.pReadDeq);
    if (pThis->tVars.disk.pReadDel != NULL) strm.Destruct(&pThis->tVars.disk.pReadDel);
}

static void qqueueResetRecoveredQueueSize(qqueue_t *pThis, const sbool adjustOverallQueueSize) {
#ifndef ENABLE_IMDIAG
    (void)adjustOverallQueueSize;
#endif
    if (pThis->iQueueSize > 0) {
#ifdef ENABLE_IMDIAG
        if (adjustOverallQueueSize) {
    #ifdef HAVE_ATOMIC_BUILTINS
            ATOMIC_SUB(&iOverallQueueSize, pThis->iQueueSize, &NULL);
    #else
            iOverallQueueSize -= pThis->iQueueSize; /* racy, but we can't wait for a mutex! */
    #endif
        }
#endif
        pThis->iQueueSize = 0;
    }
    pThis->nLogDeq = 0;
    if (pThis->qType == QUEUETYPE_DISK || pThis->bIsDA) {
        pThis->tVars.disk.sizeOnDisk = 0;
    }
}

static void qqueueSubtractOverallQueueSize(const int nElem) {
    if (nElem <= 0) {
        return;
    }
#ifdef ENABLE_IMDIAG
    #ifdef HAVE_ATOMIC_BUILTINS
    ATOMIC_SUB(&iOverallQueueSize, nElem, &NULL);
    #else
    iOverallQueueSize -= nElem; /* racy, but we can't wait for a mutex! */
    #endif
#endif
}

static void qqueueAddOverallQueueSize(const int nElem) {
    if (nElem <= 0) {
        return;
    }
#ifdef ENABLE_IMDIAG
    #ifdef HAVE_ATOMIC_BUILTINS
    ATOMIC_ADD(iOverallQueueSize, nElem);
    #else
    iOverallQueueSize += nElem; /* racy, but we can't wait for a mutex! */
    #endif
#endif
}

static void qqueueAddLogDeq(qqueue_t *pThis, const int nElem) {
#ifdef HAVE_ATOMIC_BUILTINS
    ATOMIC_ADD(pThis->nLogDeq, nElem);
#else
    pthread_mutex_lock(&pThis->mutLogDeq);
    pThis->nLogDeq += nElem;
    pthread_mutex_unlock(&pThis->mutLogDeq);
#endif
}

static void qqueueAddPhysicalQueueSize(qqueue_t *pThis, const int nElem) {
#ifdef HAVE_ATOMIC_BUILTINS
    ATOMIC_ADD(pThis->iQueueSize, nElem);
#else
    pthread_mutex_lock(&pThis->mutQueueSize);
    pThis->iQueueSize += nElem;
    pthread_mutex_unlock(&pThis->mutQueueSize);
#endif
}

/**
 * @brief Open the segmented store backing a pure or disk-assisted queue.
 *
 * The same store implementation backs eager pure segmentedDisk queues and
 * lazy DA children. For the latter, lazy_create allows open to succeed without
 * creating files; qAddSegDisk() materializes them when overflow actually
 * occurs. Recovered logical count is reflected in both child and overall queue
 * accounting before workers are advised.
 */
static rsRetVal qConstructSegDisk(qqueue_t *pThis) {
    segdisk_store_config_t cfg = {
        .work_dir = (const char *)pThis->pszSpoolDir,
        .file_prefix = (const char *)pThis->pszFilePrefix,
        .queue_name = (const char *)obj.GetName((obj_t *)pThis),
        .max_file_size = pThis->iMaxFileSize,
        .max_disk_space = pThis->sizeOnDiskMax,
        .checkpoint_interval = (unsigned int)pThis->iPersistUpdCnt,
        .sync_files = pThis->bSyncQueueFiles,
        .lazy_create = pThis->segdiskLazyCreate,
    };
    int recovered = 0;
    DEFiRet;

    CHKiRet(segdiskStoreOpen(&pThis->tVars.segdisk, &cfg, &recovered));
    pThis->iQueueSize = recovered;
    qqueueAddOverallQueueSize(recovered);
    qqueueUpdateSegDiskStats(pThis);

finalize_it:
    if (iRet != RS_RET_OK)
        LogError(0, iRet, "%s: segmentedDisk store initialization failed", obj.GetName((obj_t *)pThis));
    RETiRet;
}

static rsRetVal qDestructSegDisk(qqueue_t *pThis) {
    return segdiskStoreClose(&pThis->tVars.segdisk, getPhysicalQueueSize(pThis) == 0);
}

/**
 * @brief Serialize one message into a segmented store and account for DA activity.
 *
 * A lazy DA child must durably claim segmentedDisk before its first append can
 * create .segq. The marker write therefore precedes serialization and remains
 * pending after a failure. A successful DA append increments the parent
 * activity generation, invalidating any in-progress idle-dematerialization
 * grace period. The store consumes no message reference, so this function
 * always destroys the queue-owned reference before returning.
 */
static rsRetVal qAddSegDisk(qqueue_t *pThis, smsg_t *pMsg) {
    DEFiRet;
    if (pThis->daEngineMarkerPending) {
        /* The marker lives in workDirectory beside <prefix>.segq, not inside
         * that lazy store directory.  StartDA has already validated the
         * parent spool directory, so publishing the engine choice here can
         * and must precede segdiskStoreAppend() materializing .segq. */
        CHKiRet(qdaEngineWriteMarker((const char *)pThis->pszSpoolDir, (const char *)pThis->pszFilePrefix,
                                     QDA_ENGINE_SEGMENTED_DISK));
        pThis->daEngineMarkerPending = 0;
    }
    CHKiRet(segdiskStoreAppend(pThis->tVars.segdisk, pMsg, 0, NULL));
    if (pThis->segdiskDAChild) ++pThis->pqParent->daActivityGeneration;
    qqueueUpdateSegDiskStats(pThis);
finalize_it:
    /* The store serializes synchronously and never retains the message. The
     * queue owns the reference even when serialization or I/O fails. */
    msgDestruct(&pMsg);
    RETiRet;
}

static rsRetVal qDeqBatchSegDisk(qqueue_t *pThis, batch_t *batch, int max, int *skipped) {
    int discovered = 0;
    const rsRetVal store_ret = segdiskStoreDequeueBatch(pThis->tVars.segdisk, batch, max, skipped, &discovered);
    if (discovered > 0) {
        qqueueAddPhysicalQueueSize(pThis, discovered);
        qqueueAddOverallQueueSize(discovered);
        if (pThis->pqParent != NULL) qqueueLocalDiskRestored(pThis->pqParent, (uint64_t)discovered);
    }
    qqueueUpdateSegDiskStats(pThis);
    return store_ret == RS_RET_RETRY ? RS_RET_NO_DATA : store_ret;
}

static rsRetVal qCompleteBatchSegDisk(qqueue_t *pThis, batch_t *batch, int *committed, int *retried) {
    const rsRetVal r = segdiskStoreCompleteBatch(pThis->tVars.segdisk, batch, committed, retried);
    if (r == RS_RET_OK && pThis->segdiskDAChild && segdiskStoreCanDematerialize(pThis->tVars.segdisk))
        pThis->segdiskIdleObservedActivity = pThis->pqParent->daActivityGeneration;
    qqueueUpdateSegDiskStats(pThis);
    return r;
}
static rsRetVal qqueueSwitchToInMemoryEmergency(qqueue_t *pThis) {
    DEFiRet;
    qqueueResetRecoveredQueueSize(pThis, 1);
    qqueueDestroyDiskStreams(pThis);
    free(pThis->pszFilePrefix);
    pThis->pszFilePrefix = NULL;
    pThis->lenFilePrefix = 0;
    pThis->bIsDA = 0;
    free(pThis->pszQIFNam);
    pThis->pszQIFNam = NULL;
    pThis->lenQIFNam = 0;
    pThis->bNeedDelQIF = 0;
    pThis->qType = QUEUETYPE_LINKEDLIST;
    qqueueSetupLinkedList(pThis);
    CHKiRet(pThis->qConstruct(pThis));
    LogMsg(0, RS_RET_ERR, LOG_ALERT,
           "Queue now runs in pure in-memory emergency mode. Data is not persistent and can be lost "
           "on restart or crash.");
finalize_it:
    RETiRet;
}

typedef struct fileEntry_s {
    int number;
    char *name;
} fileEntry_t;

static int fileEntryCmpByNumber(const void *a, const void *b) {
    const fileEntry_t *e1 = (const fileEntry_t *)a;
    const fileEntry_t *e2 = (const fileEntry_t *)b;
    if (e1->number < e2->number) return -1;
    if (e1->number > e2->number) return 1;
    return 0;
}

static sbool fileEntryExistsByNumber(const fileEntry_t *files, int nFiles, int number) {
    fileEntry_t key;
    if (nFiles <= 0 || files == NULL) {
        return 0;
    }
    key.number = number;
    key.name = NULL;
    return bsearch(&key, files, (size_t)nFiles, sizeof(fileEntry_t), fileEntryCmpByNumber) != NULL;
}

static sbool qqueueLoadRetIsStructuralCorruption(rsRetVal loadRet) {
    /* This classifier is used only by qConstructDisk() after parsing classic
     * .qi state. Segmented state and DA engine markers have separate fail-fast
     * validation paths and never reach it. Do not classify the generic
     * RS_RET_INVALID_VALUE here: it is not a classic deserializer framing
     * result and may represent a non-corruption startup/configuration error. */
    static const rsRetVal structural_errors[] = {
        RS_RET_FILE_TRUNCATED,      RS_RET_EOF,
        RS_RET_INVALID_OID,         RS_RET_INVALID_HEADER,
        RS_RET_INVALID_HEADER_VERS, RS_RET_INVALID_DELIMITER,
        RS_RET_INVALID_PROPFRAME,   RS_RET_NO_PROPLINE,
        RS_RET_INVALID_TRAILER,     RS_RET_INVALID_HEADER_RECTYPE,
        RS_RET_QTYPE_MISMATCH,      RS_RET_SYNTAX_ERROR,
        RS_RET_DS_PROP_SEQ_ERR,     RS_RET_INVLD_PROP,
    };
    for (size_t i = 0; i < sizeof(structural_errors) / sizeof(structural_errors[0]); ++i) {
        if (loadRet == structural_errors[i]) return 1;
    }
    return 0;
}

static rsRetVal qqueueVerifyAndRecover(qqueue_t *pThis, rsRetVal loadRet) {
    DEFiRet;
    DIR *d = NULL;
    struct dirent *dir;
    fileEntry_t *files = NULL;
    int nFiles = 0;
    int capFiles = 0;
    int fileNum;
    int corruptionDetected = 0;
    int canScanDir = 1;
    int i;

    if (pThis->onCorruption == QUEUE_ON_CORRUPTION_IGNORE) {
        return loadRet;
    }

    if (pThis->pszSpoolDir == NULL || pThis->pszSpoolDir[0] == '\0' || pThis->pszFilePrefix == NULL) {
        iRet = loadRet;
        FINALIZE;
    }

    if (strchr((char *)pThis->pszFilePrefix, '/') != NULL || strchr((char *)pThis->pszFilePrefix, '\\') != NULL) {
        LogError(0, RS_RET_ERR,
                 "queue corruption: queue file prefix contains path separators; switching to emergency in-memory mode");
        CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
        iRet = RS_RET_OK;
        FINALIZE;
    }

    /* 1. Directory Scan */
    d = opendir((char *)pThis->pszSpoolDir);
    if (d) {
        while (1) {
            errno = 0;
            dir = readdir(d);
            if (dir == NULL) {
                break;
            }
            size_t nameLen = strlen(dir->d_name);
            size_t prefixLen = pThis->lenFilePrefix;
            if (nameLen > prefixLen + 1 && strncmp(dir->d_name, (char *)pThis->pszFilePrefix, prefixLen) == 0 &&
                dir->d_name[prefixLen] == '.') {
                char *suffix = dir->d_name + prefixLen + 1;
                if (strcmp(suffix, "qi") == 0) continue;

                char *endptr;
                long parsedNum;
                errno = 0;
                parsedNum = strtol(suffix, &endptr, 10);
                if (*endptr == '\0') {
                    if (errno == ERANGE || parsedNum < 0 || parsedNum > INT_MAX) {
                        LogError(0, RS_RET_ERR, "queue corruption: found file with invalid sequence number %s", suffix);
                        corruptionDetected = 1;
                        continue;
                    }
                    if (parsedNum >= MAX_DISK_QUEUE_FILES) {
                        LogError(0, RS_RET_ERR, "queue corruption: found file with out-of-range sequence number %ld",
                                 parsedNum);
                        corruptionDetected = 1;
                        continue;
                    }
                    fileNum = (int)parsedNum;
                    if (nFiles == capFiles) {
                        fileEntry_t *newFiles;
                        int newCapFiles = capFiles ? capFiles * 2 : 16;
                        if ((size_t)newCapFiles > SIZE_MAX / sizeof(fileEntry_t)) {
                            LogError(0, RS_RET_OUT_OF_MEMORY,
                                     "queue corruption: too many queue files while scanning spool directory");
                            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                        }
                        CHKmalloc(newFiles = realloc(files, (size_t)newCapFiles * sizeof(fileEntry_t)));
                        files = newFiles;
                        capFiles = newCapFiles;
                    }
                    char *dupName = strdup(dir->d_name);
                    if (dupName == NULL) {
                        iRet = RS_RET_OUT_OF_MEMORY;
                        FINALIZE;
                    }
                    files[nFiles].number = fileNum;
                    files[nFiles].name = dupName;
                    nFiles++;
                }
            }
        }
        if (errno != 0) {
            canScanDir = 0;
            LogError(errno, RS_RET_ERR,
                     "queue corruption: unable to fully scan spool directory %s; skipping corruption scan",
                     pThis->pszSpoolDir);
        }
        closedir(d);
        d = NULL;
    } else {
        canScanDir = 0;
        if (errno != ENOENT && errno != ENOTDIR) {
            LogError(errno, RS_RET_ERR, "queue corruption: unable to scan spool directory %s; skipping corruption scan",
                     pThis->pszSpoolDir);
        }
    }

    /* If we cannot scan spool files, we cannot safely validate file continuity. */
    if (!canScanDir) {
        iRet = loadRet;
        FINALIZE;
    }

    if (nFiles > 1) {
        qsort(files, (size_t)nFiles, sizeof(fileEntry_t), fileEntryCmpByNumber);
    }

    /* 2. State Validation */
    if (loadRet == RS_RET_OK) {
        int deqNum = strmGetCurrFileNum(pThis->tVars.disk.pReadDeq);
        int enqNum = strmGetCurrFileNum(pThis->tVars.disk.pWrite);

        if (deqNum < 0 || deqNum >= MAX_DISK_QUEUE_FILES) {
            LogError(0, RS_RET_ERR, "queue corruption: .qi file contains invalid dequeue number %d", deqNum);
            corruptionDetected = 1;
        }
        if (enqNum < 0 || enqNum >= MAX_DISK_QUEUE_FILES) {
            LogError(0, RS_RET_ERR, "queue corruption: .qi file contains invalid enqueue number %d", enqNum);
            corruptionDetected = 1;
        }

        if (!corruptionDetected) {
            int curr = deqNum;
            int limit = 0;
            /* Verify chain from deqNum to enqNum */
            while (curr != enqNum) {
                if (!fileEntryExistsByNumber(files, nFiles, curr)) {
                    LogError(0, RS_RET_ERR, "queue corruption: missing file in sequence: %d", curr);
                    corruptionDetected = 1;
                    break;
                }
                curr = (curr + 1) % MAX_DISK_QUEUE_FILES;
                limit++;
                if (limit > MAX_DISK_QUEUE_FILES) {
                    /* Should theoretically not happen unless strm logic is broken */
                    LogError(0, RS_RET_ERR, "queue corruption: infinite loop detected verifying sequence");
                    corruptionDetected = 1;
                    break;
                }
            }
            /* Check enqueue file itself */
            if (!corruptionDetected && !fileEntryExistsByNumber(files, nFiles, enqNum)) {
                LogError(0, RS_RET_ERR, "queue corruption: missing enqueue file: %d", enqNum);
                corruptionDetected = 1;
            }
        }

        /* Check for orphaned files (files on disk not in the active range) */
        if (!corruptionDetected) {
            int isWrapped = (enqNum < deqNum);
            for (i = 0; i < nFiles; i++) {
                int fNum = files[i].number;
                int isOrphan = 0;
                if (fNum < 0 || fNum >= MAX_DISK_QUEUE_FILES) continue; /* Defensive only; should be filtered above */

                if (!isWrapped) {
                    if (fNum < deqNum || fNum > enqNum) isOrphan = 1;
                } else {
                    if (fNum > enqNum && fNum < deqNum) isOrphan = 1;
                }

                if (isOrphan) {
                    LogError(0, RS_RET_ERR,
                             "queue corruption: orphaned file found: %s (seq %d not in range [%d, %d]%s)",
                             files[i].name, fNum, deqNum, enqNum, isWrapped ? " wrapped" : "");
                    corruptionDetected = 1;
                }
            }
        }

    } else {
        /* If .qi is missing/corrupt but we have segment files */
        if (nFiles > 0) {
            corruptionDetected = 1;
            LogError(0, RS_RET_ERR, "queue corruption: .qi file missing or inaccessible but %d segment files exist",
                     nFiles);
        } else if (qqueueLoadRetIsStructuralCorruption(loadRet)) {
            /* Only deterministic parser/framing failures prove that the .qi
             * contents are corrupt.  I/O, access, allocation, and other
             * operational failures retain loadRet and fail startup so a
             * transient storage problem can never quarantine healthy state. */
            corruptionDetected = 1;
            LogError(0, RS_RET_ERR, "queue corruption: .qi file is invalid and contains no usable queue state");
        }
    }

    /* 3. Recovery */
    if (corruptionDetected) {
        if (pThis->onCorruption == QUEUE_ON_CORRUPTION_IN_MEMORY) {
            LogMsg(0, RS_RET_ERR, LOG_ALERT,
                   "Queue corruption detected. Entering emergency in-memory mode (non-persistent queue).");
            CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
            iRet = RS_RET_OK;

        } else {
            /* SAFE_MODE */
            LogMsg(0, RS_RET_ERR, LOG_ALERT,
                   "Queue corruption detected! Moving queue files to .bad directory and starting fresh.");

            char timebuf[64];
            struct tm tm_buf;
            time_t now = time(NULL);
            if (now == (time_t)-1) {
                LogError(errno, RS_RET_ERR,
                         "queue corruption: time() failed during recovery directory generation, using PID fallback");
                struct timespec ts;
                if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
                    snprintf(timebuf, sizeof(timebuf), "timeerr-%ld-%ld", (long)getpid(), (long)ts.tv_nsec);
                } else {
                    snprintf(timebuf, sizeof(timebuf), "timeerr-%ld", (long)getpid());
                }
            } else {
                localtime_r(&now, &tm_buf);
                strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S", &tm_buf);
            }

            char badDir[MAXFNAME];
            int badDirLen =
                snprintf(badDir, sizeof(badDir), "%s/%s.bad.%s", pThis->pszSpoolDir, pThis->pszFilePrefix, timebuf);
            if (badDirLen < 0 || badDirLen >= (int)sizeof(badDir)) {
                LogError(0, RS_RET_ERR,
                         "queue corruption: recovery directory path too long, switching to emergency in-memory mode");
                CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
                iRet = RS_RET_OK;
                FINALIZE;
            }

            if (mkdir(badDir, 0755) != 0) {
                if (errno == EEXIST) {
                    struct stat badDirStat;
                    if (stat(badDir, &badDirStat) != 0 || !S_ISDIR(badDirStat.st_mode)) {
                        LogError(errno, RS_RET_ERR,
                                 "queue corruption: recovery directory path exists but is unusable %s; switching to "
                                 "emergency in-memory mode",
                                 badDir);
                        CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
                        iRet = RS_RET_OK;
                        FINALIZE;
                    }
                } else {
                    LogError(errno, RS_RET_ERR,
                             "queue corruption: failed to create recovery directory %s; switching to emergency "
                             "in-memory mode",
                             badDir);
                    CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
                    iRet = RS_RET_OK;
                    FINALIZE;
                }
            }

            qqueueDestroyDiskStreams(pThis);

            char oldPath[MAXFNAME];
            char newPath[MAXFNAME];
            int pathLen1 = snprintf(oldPath, sizeof(oldPath), "%s/%s.qi", pThis->pszSpoolDir, pThis->pszFilePrefix);
            int pathLen2 = snprintf(newPath, sizeof(newPath), "%s/%s.qi", badDir, pThis->pszFilePrefix);
            if (pathLen1 < 0 || pathLen1 >= (int)sizeof(oldPath) || pathLen2 < 0 || pathLen2 >= (int)sizeof(newPath)) {
                LogError(0, RS_RET_ERR,
                         "queue corruption: .qi recovery path too long; switching to emergency in-memory mode");
                CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
                iRet = RS_RET_OK;
                FINALIZE;
            }
            struct stat sb;
            if (stat(oldPath, &sb) == 0) {
                if (rename(oldPath, newPath) != 0) {
                    LogError(errno, RS_RET_ERR,
                             "queue corruption: could not move .qi file to recovery directory; switching to emergency "
                             "in-memory mode");
                    CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
                    iRet = RS_RET_OK;
                    FINALIZE;
                }
            }

            for (i = 0; i < nFiles; i++) {
                char oldSegPath[MAXFNAME];
                char newSegPath[MAXFNAME];
                int segPathLen1 = snprintf(oldSegPath, sizeof(oldSegPath), "%s/%s", pThis->pszSpoolDir, files[i].name);
                int segPathLen2 = snprintf(newSegPath, sizeof(newSegPath), "%s/%s", badDir, files[i].name);
                if (segPathLen1 < 0 || segPathLen1 >= (int)sizeof(oldSegPath) || segPathLen2 < 0 ||
                    segPathLen2 >= (int)sizeof(newSegPath)) {
                    LogError(0, RS_RET_ERR,
                             "queue corruption: queue file recovery path too long; switching to emergency "
                             "in-memory mode");
                    CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
                    iRet = RS_RET_OK;
                    FINALIZE;
                }
                if (rename(oldSegPath, newSegPath) != 0) {
                    LogError(errno, RS_RET_ERR,
                             "queue corruption: could not move queue file %s to recovery directory; switching to "
                             "emergency in-memory mode",
                             files[i].name);
                    CHKiRet(qqueueSwitchToInMemoryEmergency(pThis));
                    iRet = RS_RET_OK;
                    FINALIZE;
                }
            }

            qqueueResetRecoveredQueueSize(pThis, 1);
            iRet = RS_RET_FILE_NOT_FOUND;
        }
    } else {
        iRet = loadRet;
    }

finalize_it:
    if (d != NULL) {
        closedir(d);
    }
    if (files) {
        for (i = 0; i < nFiles; i++) free(files[i].name);
        free(files);
    }
    RETiRet;
}

static rsRetVal makeDiskQueueBadDir(qqueue_t *pThis, char *badDir, const size_t badDirLen) {
    char timebuf[96];
    struct tm tm_buf;
    time_t now = time(NULL);
    DEFiRet;

    if (now == (time_t)-1) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
            snprintf(timebuf, sizeof(timebuf), "timeerr-%ld-%ld", (long)getpid(), (long)ts.tv_nsec);
        } else {
            snprintf(timebuf, sizeof(timebuf), "timeerr-%ld", (long)getpid());
        }
    } else {
        localtime_r(&now, &tm_buf);
        strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S", &tm_buf);
    }

    const int len = snprintf(badDir, badDirLen, "%s/%s.bad.%s.%ld", pThis->pszSpoolDir, pThis->pszFilePrefix, timebuf,
                             (long)getpid());
    if (len < 0 || len >= (int)badDirLen) {
        ABORT_FINALIZE(RS_RET_ERR);
    }
    if (mkdir(badDir, 0755) != 0) {
        if (errno != EEXIST) {
            LogError(errno, RS_RET_ERR, "queue corruption: failed to create quarantine directory %s", badDir);
            ABORT_FINALIZE(RS_RET_ERR);
        }
        struct stat badDirStat;
        if (stat(badDir, &badDirStat) != 0 || !S_ISDIR(badDirStat.st_mode)) {
            LogError(errno, RS_RET_ERR, "queue corruption: quarantine path exists but is unusable %s", badDir);
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }

finalize_it:
    RETiRet;
}

static rsRetVal moveQueueFileToBadDir(qqueue_t *pThis, const char *badDir, const char *fileName) {
    char oldPath[MAXFNAME];
    char newPath[MAXFNAME];
    DEFiRet;

    const int oldLen = snprintf(oldPath, sizeof(oldPath), "%s/%s", pThis->pszSpoolDir, fileName);
    const int newLen = snprintf(newPath, sizeof(newPath), "%s/%s", badDir, fileName);
    if (oldLen < 0 || oldLen >= (int)sizeof(oldPath) || newLen < 0 || newLen >= (int)sizeof(newPath)) {
        ABORT_FINALIZE(RS_RET_ERR);
    }
    if (rename(oldPath, newPath) != 0 && errno != ENOENT) {
        LogError(errno, RS_RET_ERR, "queue corruption: could not move queue file %s to quarantine directory %s",
                 fileName, badDir);
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal qqueueQuarantineCurrentDiskFiles(qqueue_t *pThis, char *badDir, const size_t badDirLen) {
    DIR *d = NULL;
    struct dirent *dir;
    fileEntry_t *files = NULL;
    int nFiles = 0;
    int capFiles = 0;
    int i;
    DEFiRet;

    CHKiRet(makeDiskQueueBadDir(pThis, badDir, badDirLen));
    d = opendir((char *)pThis->pszSpoolDir);
    if (d == NULL) {
        LogError(errno, RS_RET_ERR, "queue corruption: unable to scan spool directory %s for quarantine",
                 pThis->pszSpoolDir);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    errno = 0;
    while ((dir = readdir(d)) != NULL) {
        const size_t nameLen = strlen(dir->d_name);
        const size_t prefixLen = pThis->lenFilePrefix;
        if (nameLen <= prefixLen + 1 || strncmp(dir->d_name, (char *)pThis->pszFilePrefix, prefixLen) != 0 ||
            dir->d_name[prefixLen] != '.') {
            continue;
        }
        const char *suffix = dir->d_name + prefixLen + 1;
        if (strcmp(suffix, "qi") == 0) {
            continue;
        }
        char *endptr;
        errno = 0;
        const long parsedNum = strtol(suffix, &endptr, 10);
        if (*endptr != '\0' || errno == ERANGE || parsedNum < 0 || parsedNum > INT_MAX) {
            continue;
        }
        if (nFiles == capFiles) {
            fileEntry_t *newFiles;
            const int newCapFiles = capFiles ? capFiles * 2 : 16;
            if ((size_t)newCapFiles > SIZE_MAX / sizeof(fileEntry_t)) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            CHKmalloc(newFiles = realloc(files, (size_t)newCapFiles * sizeof(fileEntry_t)));
            files = newFiles;
            capFiles = newCapFiles;
        }
        CHKmalloc(files[nFiles].name = strdup(dir->d_name));
        files[nFiles].number = (int)parsedNum;
        ++nFiles;
    }
    if (errno != 0) {
        LogError(errno, RS_RET_ERR, "queue corruption: unable to fully scan spool directory %s for quarantine",
                 pThis->pszSpoolDir);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    closedir(d);
    d = NULL;

    qqueueDestroyDiskStreams(pThis);

    char qiName[MAXFNAME];
    const int qiLen = snprintf(qiName, sizeof(qiName), "%s.qi", pThis->pszFilePrefix);
    if (qiLen < 0 || qiLen >= (int)sizeof(qiName)) {
        ABORT_FINALIZE(RS_RET_ERR);
    }
    CHKiRet(moveQueueFileToBadDir(pThis, badDir, qiName));
    for (i = 0; i < nFiles; ++i) {
        CHKiRet(moveQueueFileToBadDir(pThis, badDir, files[i].name));
    }

finalize_it:
    if (d != NULL) {
        closedir(d);
    }
    if (files != NULL) {
        for (i = 0; i < nFiles; ++i) free(files[i].name);
        free(files);
    }
    RETiRet;
}

static rsRetVal qqueueResetDiskQueueAfterCorruption(qqueue_t *pThis) {
    char badDir[MAXFNAME];
    DEFiRet;

    CHKiRet(qqueueQuarantineCurrentDiskFiles(pThis, badDir, sizeof(badDir)));
    LogMsg(0, RS_RET_ERR, LOG_ALERT,
           "%s: disk queue corruption could not be resynchronized; quarantined unread queue tail into %s",
           obj.GetName((obj_t *)pThis), badDir);

    qqueueResetRecoveredQueueSize(pThis, 0);
    pThis->tVars.disk.sizeOnDisk = 0;
    pThis->tVars.disk.deqFileNumIn = 0;
    pThis->tVars.disk.deqFileNumOut = 0;
    pThis->tVars.disk.deqOffs = 0;
    pThis->tVars.disk.nForcePersist = 0;
    pThis->tVars.disk.pendingCorruptRet = RS_RET_OK;
    pThis->bNeedDelQIF = 0;
    pThis->iUpdsSincePersist = 0;
    CHKiRet(qConstructDisk(pThis));
    LogMsg(0, RS_RET_OK, LOG_WARNING, "%s: disk queue active state reset after corruption quarantine",
           obj.GetName((obj_t *)pThis));

finalize_it:
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "%s: disk queue corruption quarantine failed; switching to emergency in-memory mode",
                 obj.GetName((obj_t *)pThis));
        const rsRetVal localRet = qqueueSwitchToInMemoryEmergency(pThis);
        if (localRet != RS_RET_OK) {
            LogError(0, localRet, "%s: emergency in-memory mode switch failed after disk queue corruption",
                     obj.GetName((obj_t *)pThis));
        }
        iRet = RS_RET_OK;
    }
    RETiRet;
}

/* The following function is used to "save" ourself from being killed by
 * a fatally failed disk queue. A fatal failure is, for example, if no
 * data can be read or written. In that case, the disk support is disabled,
 * with all on-disk structures kept as-is as much as possible. However,
 * we do not really stop or destruct the in-memory disk queue object.
 * Practice has shown that this may cause races during destruction which
 * themselves can lead to segfault. So we prefer to was some resources by
 * keeping the queue active.
 * Instead, the queue is switched to direct mode, so that at least
 * some processing can happen. Of course, this may still have lots of
 * undesired side-effects, but is probably better than aborting the
 * syslogd. Note that this function *must* succeed in one way or another, as
 * we can not recover from failure here. But it may emit different return
 * states, which can trigger different processing in the higher layers.
 * rgerhards, 2011-05-03
 */
static rsRetVal queueSwitchToEmergencyMode(qqueue_t *pThis, rsRetVal initiatingError) {
    const int graphMember = pThis->localGraphConf != NULL;
    if (graphMember) qqueueLock(pThis);
    ATOMIC_STORE_32BIT(&pThis->iQueueSize, &pThis->mutQueueSize, 0);
    ATOMIC_STORE_32BIT(&pThis->nLogDeq, &pThis->mutLogDeq, 0);

    pThis->qType = QUEUETYPE_DIRECT;
    pThis->qConstruct = qConstructDirect;
    pThis->qDestruct = qDestructDirect;
    /* these entry points shall not be used in direct mode
     * To catch program errors, make us abort if that happens!
     * rgerhards, 2013-11-05
     */
    pThis->qAdd = qAddDirect;
    pThis->MultiEnq = qqueueMultiEnqObjDirect;
    pThis->qDel = NULL;
    if (pThis->pqParent != NULL) {
        DBGOPRINT((obj_t *)pThis, "DA queue is in emergency mode, disabling DA in parent\n");
        pThis->pqParent->bIsDA = 0;
        if (pThis->localGraphConf != NULL) pThis->pqParent->localRetiredDA = pThis;
        __atomic_store_n(&pThis->pqParent->pqDA, NULL, __ATOMIC_RELEASE);
        /* This may have undesired side effects, not sure if I really evaluated
         * all. So you know where to look at if you come to this point during
         * troubleshooting ;) -- rgerhards, 2011-05-03
         */
    }

    if (graphMember) d_pthread_mutex_unlock(pThis->mut);
    LogError(0, initiatingError,
             "fatal error on disk queue '%s', "
             "emergency switch to direct mode",
             obj.GetName((obj_t *)pThis));
    return RS_RET_ERR_QUEUE_EMERGENCY;
}


static rsRetVal qqueueLoadPersStrmInfoFixup(strm_t *pStrm, qqueue_t __attribute__((unused)) * pThis) {
    DEFiRet;
    ISOBJ_TYPE_assert(pStrm, strm);
    ISOBJ_TYPE_assert(pThis, qqueue);
    CHKiRet(strm.SetDir(pStrm, pThis->pszSpoolDir, pThis->lenSpoolDir));
    CHKiRet(strm.SetbSync(pStrm, pThis->bSyncQueueFiles));
    CHKiRet(strm.SetbNoFollowFinal(pStrm, 1));
finalize_it:
    RETiRet;
}


/* The method loads the persistent queue information.
 * rgerhards, 2008-01-11
 */
static rsRetVal qqueueTryLoadPersistedInfo(qqueue_t *pThis) {
    DEFiRet;
    strm_t *psQIF = NULL;
    struct stat stat_buf;

    ISOBJ_TYPE_assert(pThis, qqueue);

    /* check if the file exists */
    if (stat((char *)pThis->pszQIFNam, &stat_buf) == -1) {
        if (errno == ENOENT) {
            DBGOPRINT((obj_t *)pThis, "clean startup, no .qi file found\n");
            ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
        } else {
            LogError(errno, RS_RET_IO_ERROR, "queue: %s: error %d could not access .qi file",
                     obj.GetName((obj_t *)pThis), errno);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
    }

    /* If we reach this point, we have a .qi file */

    CHKiRet(strm.Construct(&psQIF));
    CHKiRet(strm.SettOperationsMode(psQIF, STREAMMODE_READ));
    CHKiRet(strm.SetsType(psQIF, STREAMTYPE_FILE_SINGLE));
    CHKiRet(strm.SetbNoFollowFinal(psQIF, 1));
    CHKiRet(strm.SetFName(psQIF, pThis->pszQIFNam, pThis->lenQIFNam));
    CHKiRet(strm.ConstructFinalize(psQIF));

    /* first, we try to read the property bag for ourselfs */
    CHKiRet(obj.DeserializePropBag((obj_t *)pThis, psQIF));

    /* then the stream objects (same order as when persisted!) */
    CHKiRet(obj.Deserialize(&pThis->tVars.disk.pWrite, (uchar *)"strm", psQIF,
                            (rsRetVal(*)(obj_t *, void *))qqueueLoadPersStrmInfoFixup, pThis));
    CHKiRet(obj.Deserialize(&pThis->tVars.disk.pReadDel, (uchar *)"strm", psQIF,
                            (rsRetVal(*)(obj_t *, void *))qqueueLoadPersStrmInfoFixup, pThis));
    /* create a duplicate for the read "pointer". */
    CHKiRet(strm.Dup(pThis->tVars.disk.pReadDel, &pThis->tVars.disk.pReadDeq));
    CHKiRet(strm.SetbDeleteOnClose(pThis->tVars.disk.pReadDeq, 0)); /* deq must NOT delete the files! */
    CHKiRet(strm.ConstructFinalize(pThis->tVars.disk.pReadDeq));
    /* if we use a crypto provider, we need to amend the objects with it's info */
    if (pThis->useCryprov) {
        CHKiRet(strm.Setcryprov(pThis->tVars.disk.pWrite, &pThis->cryprov));
        CHKiRet(strm.SetcryprovData(pThis->tVars.disk.pWrite, pThis->cryprovData));
        CHKiRet(strm.Setcryprov(pThis->tVars.disk.pReadDeq, &pThis->cryprov));
        CHKiRet(strm.SetcryprovData(pThis->tVars.disk.pReadDeq, pThis->cryprovData));
        CHKiRet(strm.Setcryprov(pThis->tVars.disk.pReadDel, &pThis->cryprov));
        CHKiRet(strm.SetcryprovData(pThis->tVars.disk.pReadDel, pThis->cryprovData));
    }

    CHKiRet(strm.SeekCurrOffs(pThis->tVars.disk.pWrite));
    rsRetVal seekRet = strm.SeekCurrOffs(pThis->tVars.disk.pReadDel);
    sbool read_seek_failed = 0;
    CHKiRet(handleReadSeekError(seekRet, pThis, "read/delete", &read_seek_failed));
    seekRet = strm.SeekCurrOffs(pThis->tVars.disk.pReadDeq);
    CHKiRet(handleReadSeekError(seekRet, pThis, "read", &read_seek_failed));
    if (read_seek_failed) {
        /* Align read pointer to write pointer to trigger recovery later. */
        alignReadDeqToWrite(pThis);
    }

    /* OK, we could successfully read the file, so we now can request that it be
     * deleted when we are done with the persisted information.
     */
    pThis->bNeedDelQIF = 1;
    LogMsg(0, RS_RET_OK, LOG_INFO,
           "%s: queue files exist on disk, re-starting with "
           "%d messages. This will keep the disk queue file open, details: "
           "https://rainer.gerhards.net/2013/07/rsyslog-why-disk-assisted-queues-keep-a-file-open.html",
           objGetName((obj_t *)pThis), getLogicalQueueSize(pThis));

finalize_it:
    if (psQIF != NULL) strm.Destruct(&psQIF);

    if (iRet != RS_RET_OK) {
        DBGOPRINT((obj_t *)pThis, "state %d reading .qi file - can not read persisted info (if any)\n", iRet);
    }

    RETiRet;
}


/* disk queue constructor.
 * Note that we use a file limit of 10,000,000 files. That number should never pose a
 * problem. If so, I guess the user has a design issue... But of course, the code can
 * always be changed (though it would probably be more appropriate to increase the
 * allowed file size at this point - that should be a config setting...
 * rgerhards, 2008-01-10
 */
static rsRetVal qConstructDisk(qqueue_t *pThis) {
    DEFiRet;
    int bRestarted = 0;

    assert(pThis != NULL);
    pThis->tVars.disk.pendingCorruptRet = RS_RET_OK;
    pThis->tVars.disk.runtimeCorruptionSkip = 0;

    /* and now check if there is some persistent information that needs to be read in */
    iRet = qqueueTryLoadPersistedInfo(pThis);

    iRet = qqueueVerifyAndRecover(pThis, iRet);

    if (iRet == RS_RET_OK && pThis->qType == QUEUETYPE_LINKEDLIST) {
        /* we switched to in-memory mode, so we are done */
        FINALIZE;
    }

    if (iRet == RS_RET_OK)
        bRestarted = 1;
    else if (iRet != RS_RET_FILE_NOT_FOUND)
        FINALIZE;

    if (bRestarted == 1) {
        ;
    } else {
        CHKiRet(strm.Construct(&pThis->tVars.disk.pWrite));
        CHKiRet(strm.SetbSync(pThis->tVars.disk.pWrite, pThis->bSyncQueueFiles));
        CHKiRet(strm.SetDir(pThis->tVars.disk.pWrite, pThis->pszSpoolDir, pThis->lenSpoolDir));
        CHKiRet(strm.SetiMaxFiles(pThis->tVars.disk.pWrite, MAX_DISK_QUEUE_FILES));
        CHKiRet(strm.SettOperationsMode(pThis->tVars.disk.pWrite, STREAMMODE_WRITE));
        CHKiRet(strm.SetsType(pThis->tVars.disk.pWrite, STREAMTYPE_FILE_CIRCULAR));
        CHKiRet(strm.SetbNoFollowFinal(pThis->tVars.disk.pWrite, 1));
        if (pThis->useCryprov) {
            CHKiRet(strm.Setcryprov(pThis->tVars.disk.pWrite, &pThis->cryprov));
            CHKiRet(strm.SetcryprovData(pThis->tVars.disk.pWrite, pThis->cryprovData));
        }
        CHKiRet(strm.ConstructFinalize(pThis->tVars.disk.pWrite));

        CHKiRet(strm.Construct(&pThis->tVars.disk.pReadDeq));
        CHKiRet(strm.SetbDeleteOnClose(pThis->tVars.disk.pReadDeq, 0));
        CHKiRet(strm.SetDir(pThis->tVars.disk.pReadDeq, pThis->pszSpoolDir, pThis->lenSpoolDir));
        CHKiRet(strm.SetiMaxFiles(pThis->tVars.disk.pReadDeq, MAX_DISK_QUEUE_FILES));
        CHKiRet(strm.SettOperationsMode(pThis->tVars.disk.pReadDeq, STREAMMODE_READ));
        CHKiRet(strm.SetsType(pThis->tVars.disk.pReadDeq, STREAMTYPE_FILE_CIRCULAR));
        CHKiRet(strm.SetbNoFollowFinal(pThis->tVars.disk.pReadDeq, 1));
        if (pThis->useCryprov) {
            CHKiRet(strm.Setcryprov(pThis->tVars.disk.pReadDeq, &pThis->cryprov));
            CHKiRet(strm.SetcryprovData(pThis->tVars.disk.pReadDeq, pThis->cryprovData));
        }
        CHKiRet(strm.ConstructFinalize(pThis->tVars.disk.pReadDeq));

        CHKiRet(strm.Construct(&pThis->tVars.disk.pReadDel));
        CHKiRet(strm.SetbSync(pThis->tVars.disk.pReadDel, pThis->bSyncQueueFiles));
        CHKiRet(strm.SetbDeleteOnClose(pThis->tVars.disk.pReadDel, 1));
        CHKiRet(strm.SetDir(pThis->tVars.disk.pReadDel, pThis->pszSpoolDir, pThis->lenSpoolDir));
        CHKiRet(strm.SetiMaxFiles(pThis->tVars.disk.pReadDel, MAX_DISK_QUEUE_FILES));
        CHKiRet(strm.SettOperationsMode(pThis->tVars.disk.pReadDel, STREAMMODE_READ));
        CHKiRet(strm.SetsType(pThis->tVars.disk.pReadDel, STREAMTYPE_FILE_CIRCULAR));
        CHKiRet(strm.SetbNoFollowFinal(pThis->tVars.disk.pReadDel, 1));
        if (pThis->useCryprov) {
            CHKiRet(strm.Setcryprov(pThis->tVars.disk.pReadDel, &pThis->cryprov));
            CHKiRet(strm.SetcryprovData(pThis->tVars.disk.pReadDel, pThis->cryprovData));
        }
        CHKiRet(strm.ConstructFinalize(pThis->tVars.disk.pReadDel));

        CHKiRet(strm.SetFName(pThis->tVars.disk.pWrite, pThis->pszFilePrefix, pThis->lenFilePrefix));
        CHKiRet(strm.SetFName(pThis->tVars.disk.pReadDeq, pThis->pszFilePrefix, pThis->lenFilePrefix));
        CHKiRet(strm.SetFName(pThis->tVars.disk.pReadDel, pThis->pszFilePrefix, pThis->lenFilePrefix));
    }

    /* now we set (and overwrite in case of a persisted restart) some parameters which
     * should always reflect the current configuration variables. Be careful by doing so,
     * for example file name generation must not be changed as that would break the
     * ability to read existing queue files. -- rgerhards, 2008-01-12
     */
    CHKiRet(strm.SetiMaxFileSize(pThis->tVars.disk.pWrite, pThis->iMaxFileSize));
    CHKiRet(strm.SetiMaxFileSize(pThis->tVars.disk.pReadDeq, pThis->iMaxFileSize));
    CHKiRet(strm.SetiMaxFileSize(pThis->tVars.disk.pReadDel, pThis->iMaxFileSize));

finalize_it:
    RETiRet;
}


static rsRetVal qDestructDisk(qqueue_t *pThis) {
    DEFiRet;

    assert(pThis != NULL);

    free(pThis->pszQIFNam);
    if (pThis->tVars.disk.pWrite != NULL) {
        if (getPhysicalQueueSize(pThis) == 0) {
            /* Once the disk queue is logically empty, any remaining active write
             * file only contains untracked tail data, typically late internal
             * messages emitted during shutdown. Keep teardown from leaving that
             * orphaned file behind.
             */
            strm.SetbDeleteOnClose(pThis->tVars.disk.pWrite, 1);
        }
        strm.Destruct(&pThis->tVars.disk.pWrite);
    }
    if (pThis->tVars.disk.pReadDeq != NULL) strm.Destruct(&pThis->tVars.disk.pReadDeq);
    if (pThis->tVars.disk.pReadDel != NULL) strm.Destruct(&pThis->tVars.disk.pReadDel);

    RETiRet;
}

static rsRetVal ATTR_NONNULL(1, 2) qAddDisk(qqueue_t *const pThis, smsg_t *pMsg) {
    DEFiRet;
    ISOBJ_TYPE_assert(pThis, qqueue);
    ISOBJ_TYPE_assert(pMsg, msg);
    if (pThis->daEngineMarkerPending) {
        CHKiRet(qdaEngineWriteMarker((const char *)pThis->pszSpoolDir, (const char *)pThis->pszFilePrefix,
                                     QDA_ENGINE_DISK));
        pThis->daEngineMarkerPending = 0;
    }
    number_t nWriteCount;
    const int oldfile = strmGetCurrFileNum(pThis->tVars.disk.pWrite);

    CHKiRet(strm.SetWCntr(pThis->tVars.disk.pWrite, &nWriteCount));
    CHKiRet((objSerialize(pMsg))(pMsg, pThis->tVars.disk.pWrite));
    CHKiRet(strm.Flush(pThis->tVars.disk.pWrite));
    CHKiRet(strm.SetWCntr(pThis->tVars.disk.pWrite, NULL)); /* no more counting for now... */

    pThis->tVars.disk.sizeOnDisk += nWriteCount;

    /* we have enqueued the user element to disk. So we now need to destruct
     * the in-memory representation. The instance will be re-created upon
     * dequeue. -- rgerhards, 2008-07-09
     */
    msgDestruct(&pMsg);

    DBGOPRINT((obj_t *)pThis, "write wrote %lld octets to disk, queue disk size now %lld octets, EnqOnly:%d\n",
              nWriteCount, pThis->tVars.disk.sizeOnDisk, pThis->bEnqOnly);

    /* Did we have a change in the on-disk file? If so, we
     * should do a "robustness sync" of the .qi file to guard
     * against the most harsh consequences of kill -9 and power off.
     */
    int newfile;
    newfile = strmGetCurrFileNum(pThis->tVars.disk.pWrite);
    if (newfile != oldfile) {
        DBGOPRINT((obj_t *)pThis,
                  "current to-be-written-to file has changed from "
                  "number %d to number %d - requiring a .qi write for robustness\n",
                  oldfile, newfile);
        pThis->tVars.disk.nForcePersist = 2;
    }

finalize_it:
    RETiRet;
}


static rsRetVal msgConstructFromVoid(void **ppThis) {
    return msgConstructForDeserializer((smsg_t **)ppThis);
}

static rsRetVal msgDeserializeFromVoid(void *pObj, strm_t *pStrm) {
    return MsgDeserialize((smsg_t *)pObj, pStrm);
}

static rsRetVal qDeqDisk(qqueue_t *pThis, smsg_t **ppMsg) {
    DEFiRet;
    iRet = objDeserializeWithMethods(ppMsg, (uchar *)"msg", sizeof("msg") - 1, pThis->tVars.disk.pReadDeq, NULL, NULL,
                                     msgConstructFromVoid, NULL, msgDeserializeFromVoid);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "%s: qDeqDisk error happened at around offset %lld", obj.GetName((obj_t *)pThis),
                 (long long)pThis->tVars.disk.pReadDeq->iCurrOffs);
    }
    RETiRet;
}

static rsRetVal qDeqDiskRecoverAfterCorruption(qqueue_t *pThis,
                                               smsg_t **ppMsg,
                                               const rsRetVal corruptRet,
                                               int *const pSkippedMsgs) {
    uchar c;
    int64_t startOffs;
    int startFile;
    unsigned int scanned = 0;
    DEFiRet;

    assert(ppMsg != NULL);
    assert(pSkippedMsgs != NULL);
    *ppMsg = NULL;
    *pSkippedMsgs = 0;

    if (pThis->onCorruption != QUEUE_ON_CORRUPTION_SAFE_MODE || pThis->tVars.disk.pReadDeq == NULL) {
        ABORT_FINALIZE(corruptRet);
    }

    if (pThis->tVars.disk.pWrite != NULL &&
        strmGetCurrFileNum(pThis->tVars.disk.pReadDeq) == strmGetCurrFileNum(pThis->tVars.disk.pWrite) &&
        pThis->tVars.disk.pReadDeq->iCurrOffs >= pThis->tVars.disk.pWrite->iCurrOffs) {
        ABORT_FINALIZE(corruptRet);
    }

    startFile = strmGetCurrFileNum(pThis->tVars.disk.pReadDeq);
    startOffs = pThis->tVars.disk.pReadDeq->iCurrOffs;
    LogMsg(0, corruptRet, LOG_ALERT,
           "%s: disk queue corruption detected while dequeuing at file %d offset %lld; attempting bounded resync",
           obj.GetName((obj_t *)pThis), startFile, (long long)startOffs);

    while (scanned < DISKQUEUE_CORRUPTION_RESYNC_MAX_BYTES) {
        if (pThis->tVars.disk.pWrite != NULL &&
            strmGetCurrFileNum(pThis->tVars.disk.pReadDeq) == strmGetCurrFileNum(pThis->tVars.disk.pWrite) &&
            pThis->tVars.disk.pReadDeq->iCurrOffs >= pThis->tVars.disk.pWrite->iCurrOffs) {
            break;
        }

        iRet = strm.ReadChar(pThis->tVars.disk.pReadDeq, &c);
        if (iRet != RS_RET_OK) {
            break;
        }
        ++scanned;
        if (c != '<') {
            continue;
        }

        CHKiRet(strm.UnreadChar(pThis->tVars.disk.pReadDeq, c));
        iRet = objDeserializeWithMethods(ppMsg, (uchar *)"msg", sizeof("msg") - 1, pThis->tVars.disk.pReadDeq, NULL,
                                         NULL, msgConstructFromVoid, NULL, msgDeserializeFromVoid);
        if (iRet == RS_RET_OK) {
            *pSkippedMsgs = 1;
            LogMsg(0, RS_RET_OK, LOG_WARNING,
                   "%s: disk queue corruption recovery resumed after skipping a damaged record; "
                   "resynchronized at file %d offset %lld after scanning %u bytes",
                   obj.GetName((obj_t *)pThis), strmGetCurrFileNum(pThis->tVars.disk.pReadDeq),
                   (long long)pThis->tVars.disk.pReadDeq->iCurrOffs, scanned);
            FINALIZE;
        }
        if (iRet == RS_RET_OUT_OF_MEMORY) {
            ABORT_FINALIZE(iRet);
        }
        *ppMsg = NULL;
    }

    LogMsg(0, corruptRet, LOG_ALERT,
           "%s: disk queue corruption recovery failed after bounded scan of %u bytes; quarantining unread tail",
           obj.GetName((obj_t *)pThis), scanned);
    const int skippedTail = getLogicalQueueSize(pThis);
    CHKiRet(qqueueResetDiskQueueAfterCorruption(pThis));
    *pSkippedMsgs = skippedTail;
    iRet = RS_RET_NO_DATA;

finalize_it:
    RETiRet;
}


/* -------------------- direct (no queueing) -------------------- */
static rsRetVal qConstructDirect(qqueue_t __attribute__((unused)) * pThis) {
    return RS_RET_OK;
}


static rsRetVal qDestructDirect(qqueue_t __attribute__((unused)) * pThis) {
    return RS_RET_OK;
}

static rsRetVal qAddDirectWithWti(qqueue_t *pThis, smsg_t *pMsg, wti_t *pWti) {
    batch_t singleBatch;
    batch_obj_t batchObj;
    batch_state_t batchState = BATCH_STATE_RDY;
    DEFiRet;

    // TODO: init batchObj (states _OK and new fields -- CHECK)
    assert(pThis != NULL);

    /* calling the consumer is quite different here than it is from a worker thread */
    /* we need to provide the consumer's return value back to the caller because in direct
     * mode the consumer probably has a lot to convey (which get's lost in the other modes
     * because they are asynchronous. But direct mode is deliberately synchronous.
     * rgerhards, 2008-02-12
     * We use our knowledge about the batch_t structure below, but without that, we
     * pay a too-large performance toll... -- rgerhards, 2009-04-22
     */
    memset(&batchObj, 0, sizeof(batch_obj_t));
    memset(&singleBatch, 0, sizeof(batch_t));
    batchObj.pMsg = pMsg;
    singleBatch.nElem = 1; /* there always is only one in direct mode */
    singleBatch.pElem = &batchObj;
    singleBatch.eltState = &batchState;
    iRet = pThis->pConsumer(pThis->pAction, &singleBatch, pWti);
    msgDestruct(&pMsg);

    RETiRet;
}

/* this is called if we do not have a pWti. This currently only happens
 * when we are called from a main queue in direct mode. If so, we need
 * to obtain a dummy pWti.
 */
static rsRetVal qAddDirect(qqueue_t *pThis, smsg_t *pMsg) {
    wti_t *pWti;
    DEFiRet;

    pWti = wtiGetDummy();
    qqueueSetWtiShutdownImmediate(pThis, pWti);
    iRet = qAddDirectWithWti(pThis, pMsg, pWti);
    RETiRet;
}


/* --------------- end type-specific handlers -------------------- */


/* generic code to add a queue entry
 * We use some specific code to most efficiently support direct mode
 * queues. This is justified in spite of the gain and the need to do some
 * things truely different. -- rgerhards, 2008-02-12
 */
static rsRetVal qqueueAdd(qqueue_t *pThis, smsg_t *pMsg) {
    DEFiRet;

    assert(pThis != NULL);

    static int msgCnt = 0;

    if (pThis->iSmpInterval > 0) {
        msgCnt = (msgCnt + 1) % (pThis->iSmpInterval);
        if (msgCnt != 0) {
            msgDestruct(&pMsg);
            goto finalize_it;
        }
    }

    CHKiRet(pThis->qAdd(pThis, pMsg));

    if (pThis->bIsDA && pThis->pqDA != NULL && pThis->pqDA->segdiskDAChild) {
        /* Parent and DA child intentionally share this queue mutex. The idle
         * callback therefore observes this activity generation atomically
         * with both producer enqueue and child store operations. */
        ++pThis->daActivityGeneration;
        if (pThis->pqDA->pWtpReg != NULL) wtpWakeupAllWrkr(pThis->pqDA->pWtpReg);
    }

    if (pThis->qType != QUEUETYPE_DIRECT) {
        ATOMIC_INC(&pThis->iQueueSize, &pThis->mutQueueSize);
#ifdef ENABLE_IMDIAG
    #ifdef HAVE_ATOMIC_BUILTINS
        /* mutex is never used due to conditional compilation */
        ATOMIC_INC(&iOverallQueueSize, &NULL);
    #else
        ++iOverallQueueSize; /* racy, but we can't wait for a mutex! */
    #endif
#endif
    }

finalize_it:
    RETiRet;
}


/* Try to shut down regular and DA queue workers, within the queue timeout
 * period. That means processing continues as usual. This is the expected
 * usual case, where during shutdown those messages remaining are being
 * processed. At this point, it is acceptable that the queue can not be
 * fully depleted, that case is handled in the next step. During this phase,
 * we first shut down the main queue DA worker to prevent new data to arrive
 * at the DA queue, and then we ask the regular workers of both the Regular
 * and DA queue to try complete processing.
 * rgerhards, 2009-10-14
 */
static rsRetVal ATTR_NONNULL(1) tryShutdownWorkersWithinQueueTimeout(qqueue_t *const pThis) {
    struct timespec tTimeout;
    rsRetVal iRetLocal;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    if (pThis->local != NULL) return qqueueLocalShutdown(pThis);

    assert(pThis->pqParent == NULL); /* detect invalid calling sequence */

    if (pThis->bIsDA) {
        /* We need to lock the mutex, as otherwise we may have a race that prevents
         * us from awaking the DA worker. */
        d_pthread_mutex_lock(pThis->mut);

        /* tell regular queue DA worker to stop shuffling messages to DA queue... */
        DBGOPRINT((obj_t *)pThis, "setting EnqOnly mode for DA worker\n");
        pThis->pqDA->bEnqOnly = 1;
        wtpSetState(pThis->pWtpDA, wtpState_SHUTDOWN_IMMEDIATE);
        wtpAdviseMaxWorkers(pThis->pWtpDA, 1, DENY_WORKER_START_DURING_SHUTDOWN);
        DBGOPRINT((obj_t *)pThis, "awoke DA worker, told it to shut down.\n");

        /* also tell the DA queue worker to shut down, so that it already knows... */
        wtpSetState(pThis->pqDA->pWtpReg, wtpState_SHUTDOWN);
        wtpAdviseMaxWorkers(pThis->pqDA->pWtpReg, 1, DENY_WORKER_START_DURING_SHUTDOWN);
        /* awake its lone worker */
        DBGOPRINT((obj_t *)pThis, "awoke DA queue regular worker, told it to shut down when done.\n");

        d_pthread_mutex_unlock(pThis->mut);
    }


    /* first calculate absolute timeout - we need the absolute value here, because we need to coordinate
     * shutdown of both the regular and DA queue on *the same* timeout.
     */
    timeoutComp(&tTimeout, pThis->toQShutdown);
    DBGOPRINT((obj_t *)pThis, "trying shutdown of regular workers\n");
    iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN, &tTimeout);
    if (iRetLocal == RS_RET_TIMED_OUT) {
        LogMsg(0, RS_RET_TIMED_OUT, LOG_INFO,
               "%s: regular queue shutdown timed out on primary queue "
               "(this is OK, timeout was %d)",
               objGetName((obj_t *)pThis), pThis->toQShutdown);
    } else {
        DBGOPRINT((obj_t *)pThis, "regular queue workers shut down.\n");
    }

    /* OK, the worker for the regular queue is processed, on the the DA queue regular worker. */
    if (pThis->pqDA != NULL) {
        DBGOPRINT((obj_t *)pThis, "we have a DA queue (0x%lx), requesting its shutdown.\n", qqueueGetID(pThis->pqDA));
        /* we use the same absolute timeout as above, so we do not use more than the configured
         * timeout interval!
         */
        DBGOPRINT((obj_t *)pThis, "trying shutdown of regular worker of DA queue\n");
        iRetLocal = wtpShutdownAll(pThis->pqDA->pWtpReg, wtpState_SHUTDOWN, &tTimeout);
        if (iRetLocal == RS_RET_TIMED_OUT) {
            LogMsg(0, RS_RET_TIMED_OUT, LOG_INFO,
                   "%s: regular queue shutdown timed out on DA queue (this is OK, "
                   "timeout was %d)",
                   objGetName((obj_t *)pThis), pThis->toQShutdown);
        } else {
            DBGOPRINT((obj_t *)pThis, "DA queue worker shut down.\n");
        }
    }

    RETiRet;
}


/* Try to shut down regular and DA queue workers, within the action timeout
 * period. This aborts processing, but at the end of the current action, in
 * a well-defined manner. During this phase, we terminate all three worker
 * pools, including the regular queue DA worker if it not yet has terminated.
 * Not finishing processing all messages is OK (and expected) at this stage
 * (they may be preserved later, depending * on bSaveOnShutdown setting).
 * rgerhards, 2009-10-14
 */
static rsRetVal tryShutdownWorkersWithinActionTimeout(qqueue_t *pThis) {
    struct timespec tTimeout;
    rsRetVal iRetLocal;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    assert(pThis->pqParent == NULL); /* detect invalid calling sequence */

    /* instruct workers to finish ASAP, even if still work exists */
    DBGOPRINT((obj_t *)pThis, "trying to shutdown workers within Action Timeout");
    DBGOPRINT((obj_t *)pThis, "setting EnqOnly mode\n");
    pThis->bEnqOnly = 1;
    qqueueSetShutdownImmediate(pThis, 1);
    /* now DA queue */
    if (pThis->bIsDA) {
        pThis->pqDA->bEnqOnly = 1;
        qqueueSetShutdownImmediate(pThis->pqDA, 1);
    }

    /* now give the queue workers a last chance to gracefully shut down (based on action timeout setting) */
    timeoutComp(&tTimeout, pThis->toActShutdown);
    DBGOPRINT((obj_t *)pThis, "trying immediate shutdown of regular workers (if any)\n");
    iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
    if (iRetLocal == RS_RET_TIMED_OUT) {
        LogMsg(0, RS_RET_TIMED_OUT, LOG_INFO,
               "%s: immediate shutdown timed out on primary queue (this is acceptable and "
               "triggers cancellation)",
               objGetName((obj_t *)pThis));
    } else if (iRetLocal != RS_RET_OK) {
        LogMsg(0, iRetLocal, LOG_WARNING,
               "%s: potential internal error: unexpected return state after trying "
               "immediate shutdown of the primary queue in disk save mode. "
               "Continuing, but results are unpredictable",
               objGetName((obj_t *)pThis));
    }

    if (pThis->pqDA != NULL) {
        /* and now the same for the DA queue */
        DBGOPRINT((obj_t *)pThis, "trying immediate shutdown of DA queue workers\n");
        iRetLocal = wtpShutdownAll(pThis->pqDA->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
        if (iRetLocal == RS_RET_TIMED_OUT) {
            LogMsg(0, RS_RET_TIMED_OUT, LOG_INFO,
                   "%s: immediate shutdown timed out on DA queue (this is acceptable and "
                   "triggers cancellation)",
                   objGetName((obj_t *)pThis));
        } else if (iRetLocal != RS_RET_OK) {
            LogMsg(0, iRetLocal, LOG_WARNING,
                   "%s: potential internal error: unexpected return state after trying "
                   "immediate shutdown of the DA queue in disk save mode. "
                   "Continuing, but results are unpredictable",
                   objGetName((obj_t *)pThis));
        }

        /* and now we need to terminate the DA worker itself. We always grant it a 100ms timeout,
         * which should be sufficient and usually not be required (it is expected to have finished
         * long before while we were processing the queue timeout in shutdown phase 1).
         * rgerhards, 2009-10-14
         */
        timeoutComp(&tTimeout, 100);
        DBGOPRINT((obj_t *)pThis, "trying regular shutdown of main queue DA worker pool\n");
        iRetLocal = wtpShutdownAll(pThis->pWtpDA, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
        if (iRetLocal == RS_RET_TIMED_OUT) {
            LogMsg(0, iRetLocal, LOG_WARNING,
                   "%s: shutdown timed out on main queue DA worker pool "
                   "(this is not good, but possibly OK)",
                   objGetName((obj_t *)pThis));
        } else {
            DBGOPRINT((obj_t *)pThis, "main queue DA worker pool shut down.\n");
        }
    }

    RETiRet;
}


/* This function cancels all remaining regular workers for both the main and the DA
 * queue.
 * rgerhards, 2009-05-29
 */
static rsRetVal cancelWorkers(qqueue_t *pThis) {
    rsRetVal iRetLocal;
    struct timespec tTimeout;
    DEFiRet;

    assert(pThis->qType != QUEUETYPE_DIRECT);

    /* Now queue workers should have terminated. If not, we need to cancel them as we have applied
     * all timeout setting. If any worker in any queue still executes, its consumer is possibly
     * long-running and cancelling is the only way to get rid of it.
     */
    DBGOPRINT((obj_t *)pThis, "checking to see if we need to cancel any worker threads of the primary queue\n");
    iRetLocal = wtpCancelAll(pThis->pWtpReg, objGetName((obj_t *)pThis));
    /* ^-- returns immediately if all threads already have terminated */
    if (iRetLocal != RS_RET_OK) {
        DBGOPRINT((obj_t *)pThis,
                  "unexpected iRet state %d trying to cancel primary queue worker "
                  "threads, continuing, but results are unpredictable\n",
                  iRetLocal);
    }
    timeoutComp(&tTimeout, QUEUE_TIMEOUT_ETERNAL);
    iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
    if (iRetLocal != RS_RET_OK) {
        DBGOPRINT((obj_t *)pThis, "unexpected state %d joining cancelled primary workers\n", iRetLocal);
    }

    /* ... and now the DA queue, if it exists (should always be after the primary one) */
    if (pThis->pqDA != NULL) {
        DBGOPRINT((obj_t *)pThis,
                  "checking to see if we need to cancel any worker threads of "
                  "the DA queue\n");
        iRetLocal = wtpCancelAll(pThis->pqDA->pWtpReg, objGetName((obj_t *)pThis));
        /* returns immediately if all threads already have terminated */
        if (iRetLocal != RS_RET_OK) {
            DBGOPRINT((obj_t *)pThis,
                      "unexpected iRet state %d trying to cancel DA queue worker "
                      "threads, continuing, but results are unpredictable\n",
                      iRetLocal);
        }
        timeoutComp(&tTimeout, QUEUE_TIMEOUT_ETERNAL);
        iRetLocal = wtpShutdownAll(pThis->pqDA->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
        if (iRetLocal != RS_RET_OK) {
            DBGOPRINT((obj_t *)pThis, "unexpected state %d joining cancelled DA child workers\n", iRetLocal);
        }

        /* finally, we cancel the main queue's DA worker pool, if it still is running. It may be
         * restarted later to persist the queue. But we stop it, because otherwise we get into
         * big trouble when resetting the logical dequeue pointer. This operation can only be
         * done when *no* worker is running. So time for a shutdown... -- rgerhards, 2009-05-28
         */
        DBGOPRINT((obj_t *)pThis, "checking to see if main queue DA worker pool needs to be cancelled\n");
        wtpCancelAll(pThis->pWtpDA, objGetName((obj_t *)pThis));
        /* returns immediately if all threads already have terminated */
        timeoutComp(&tTimeout, QUEUE_TIMEOUT_ETERNAL);
        iRetLocal = wtpShutdownAll(pThis->pWtpDA, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
        if (iRetLocal != RS_RET_OK) {
            DBGOPRINT((obj_t *)pThis, "unexpected state %d joining cancelled DA transfer workers\n", iRetLocal);
        }
    }

    RETiRet;
}


/* This function shuts down all worker threads and waits until they
 * have terminated. If they timeout, they are cancelled.
 * rgerhards, 2008-01-24
 * Please note that this function shuts down BOTH the parent AND the child queue
 * in DA case. This is necessary because their timeouts are tightly coupled. Most
 * importantly, the timeouts would be applied twice (or logic be extremely
 * complex) if each would have its own shutdown. The function does not self check
 * this condition - the caller must make sure it is not called with a parent.
 * rgerhards, 2009-05-26: we do NO longer persist the queue here if bSaveOnShutdown
 * is set. This must be handled by the caller. Not doing that cleans up the queue
 * shutdown considerably. Also, older engines had a potential hang condition when
 * the DA queue was already started and the DA worker configured for infinite
 * retries and the action was during retry processing. This was a design issue,
 * which is solved as of now. Note that the shutdown now may take a little bit
 * longer, because we no longer can persist the queue in parallel to waiting
 * on worker timeouts.
 */
/* S4 stops every upstream execution pool before closing any downstream
 * admission. All nodes share the graph's absolute monotonic deadlines. Global
 * memory boundaries retain their ordinary shared store and worker identities. */
void qqueueActivateGraphNode(qqueue_t *const queue) {
    if (!queue->bQueueStarted || queue->qType == QUEUETYPE_DIRECT) return;
    pthread_mutex_lock(queue->mut);
    queue->localGraphReady = 1;
    if (queue->pqDA != NULL) {
        queue->pqDA->localGraphReady = 1;
        qqueueAdviseMaxWorkers(queue->pqDA);
    }
    qqueueAdviseMaxWorkers(queue);
    pthread_mutex_unlock(queue->mut);
}

rsRetVal qqueueShutdownBackendUntil(qqueue_t *const owner,
                                    const struct timespec *const graceful,
                                    const struct timespec *const action) {
    pthread_mutex_lock(owner->mut);
    qqueue_t *const disk = owner->pqDA != NULL ? owner->pqDA : owner->localRetiredDA;
    pthread_mutex_unlock(owner->mut);
    wtp_t *const transfer = owner->pWtpDA;
    /* This is the legacy DA shutdown entry, delayed until FE publishers and
     * workers have handed off their endpoints. No new arrivals from upstream
     * graph nodes can occur after the node's admission closes. */
    if (disk != NULL) {
        pthread_mutex_lock(owner->mut);
        disk->bEnqOnly = 1;
        pthread_mutex_unlock(owner->mut);
    }
    if (transfer != NULL) wtpRequestShutdown(transfer, wtpState_SHUTDOWN_IMMEDIATE);
    wtpRequestShutdown(owner->pWtpReg, wtpState_SHUTDOWN);
    if (disk != NULL && disk->pWtpReg != NULL) wtpRequestShutdown(disk->pWtpReg, wtpState_SHUTDOWN);
    int pending = wtpWaitShutdownUntil(owner->pWtpReg, graceful) != RS_RET_OK;
    if (disk != NULL && disk->pWtpReg != NULL && wtpWaitShutdownUntil(disk->pWtpReg, graceful) != RS_RET_OK)
        pending = 1;
    if (pending) {
        qqueueSetShutdownImmediate(owner, 1);
        wtpRequestShutdown(owner->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE);
        if (disk != NULL) {
            qqueueSetShutdownImmediate(disk, 1);
            if (disk->pWtpReg != NULL) wtpRequestShutdown(disk->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE);
        }
        qqueueLocalNoteActionPhase(owner);
        (void)wtpWaitShutdownUntil(owner->pWtpReg, action);
        if (disk != NULL && disk->pWtpReg != NULL) (void)wtpWaitShutdownUntil(disk->pWtpReg, action);
    }
    if (transfer != NULL) (void)wtpWaitShutdownUntil(transfer, action);
    wtpRequestCancelAll(owner->pWtpReg);
    if (disk != NULL && disk->pWtpReg != NULL) wtpRequestCancelAll(disk->pWtpReg);
    if (transfer != NULL) wtpRequestCancelAll(transfer);
    (void)wtpWaitShutdownUntil(owner->pWtpReg, NULL);
    if (disk != NULL && disk->pWtpReg != NULL) (void)wtpWaitShutdownUntil(disk->pWtpReg, NULL);
    if (transfer != NULL) (void)wtpWaitShutdownUntil(transfer, NULL);
    owner->localDAActive = 0;
    return RS_RET_OK;
}

rsRetVal qqueueShutdownGraphNode(qqueue_t *const queue,
                                 const struct timespec *const graceful,
                                 const struct timespec *const action) {
    if (queue->localGraphStopped) return RS_RET_OK;
    if (!queue->bQueueStarted || queue->pWtpReg == NULL) {
        queue->localGraphStopped = 1;
        return RS_RET_OK;
    }
    rsRetVal ret = RS_RET_OK;
    if (queue->local != NULL) {
        ret = qqueueLocalShutdownUntil(queue, graceful, action);
    } else {
        int oldCancel;
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldCancel);
        pthread_mutex_lock(queue->mut);
        queue->localGraphClosed = 1;
        pthread_cond_broadcast(&queue->notFull);
        pthread_cond_broadcast(&queue->belowFullDlyWtrMrk);
        pthread_cond_broadcast(&queue->belowLightDlyWtrMrk);
        pthread_mutex_unlock(queue->mut);
        ret = qqueueShutdownBackendUntil(queue, graceful, action);
        pthread_setcancelstate(oldCancel, NULL);
    }
    return ret;
}

rsRetVal qqueueFinalizeGraphNode(qqueue_t *const queue) {
    if (queue->localGraphStopped) return RS_RET_OK;
    rsRetVal ret;
    if (queue->local != NULL) {
        ret = qqueueLocalFinishShutdown(queue);
    } else {
        ret = qqueueSaveLocalBackend(queue);
        const rsRetVal diskRet = qqueueFinishLocalDisk(queue);
        if (ret == RS_RET_OK) ret = diskRet;
        /* Pure disk boundaries keep their store. Existing memory queues
         * discard only what their configured save phase did not preserve. */
        if (queue->qType == QUEUETYPE_FIXED_ARRAY || queue->qType == QUEUETYPE_LINKEDLIST) queueDrain(queue);
    }
    queue->localGraphStopped = 1;
    return ret;
}

rsRetVal ATTR_NONNULL(1) qqueueShutdownWorkers(qqueue_t *const pThis) {
    DEFiRet;
    ISOBJ_TYPE_assert(pThis, qqueue);
    if (pThis->localGraphStopped) return RS_RET_OK;
    if (pThis->localGraphConf != NULL) return rulesetShutdownLocalGraph(pThis->localGraphConf);

    if (pThis->qType == QUEUETYPE_DIRECT) {
        FINALIZE;
    }

    assert(pThis->pqParent == NULL); /* detect invalid calling sequence */

    DBGOPRINT((obj_t *)pThis, "initiating worker thread shutdown sequence %p\n", pThis);

    CHKiRet(tryShutdownWorkersWithinQueueTimeout(pThis));

    pthread_mutex_lock(pThis->mut);
    const int parent_phys_queue_size = getPhysicalQueueSize(pThis);
    const int child_phys_queue_size = pThis->pqDA == NULL ? 0 : getPhysicalQueueSize(pThis->pqDA);
    pthread_mutex_unlock(pThis->mut);
    /* The DA transfer worker can empty the memory parent while the disk child
     * still owns queued or in-flight work.  Skipping the action-completion
     * phase in that state sends the child directly to cancellation, which can
     * discard an action at the cancellation boundary.  Parent and child share
     * the queue mutex, so inspect both at one stable point. */
    if (parent_phys_queue_size > 0 || child_phys_queue_size > 0) {
        CHKiRet(tryShutdownWorkersWithinActionTimeout(pThis));
    }

    CHKiRet(cancelWorkers(pThis));

    /* All worker threads have terminated and been joined. This stable point is
     * required before save-on-shutdown may restart the DA transfer pool. */
    DBGOPRINT((obj_t *)pThis, "worker threads terminated, remaining queue size log %d, phys %d.\n",
              getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis));

finalize_it:
    RETiRet;
}

/* Constructor for the queue object
 * This constructs the data structure, but does not yet start the queue. That
 * is done by queueStart(). The reason is that we want to give the caller a chance
 * to modify some parameters before the queue is actually started.
 */
rsRetVal qqueueConstruct(qqueue_t **ppThis,
                         queueType_t qType,
                         int iWorkerThreads,
                         int iMaxQueueSize,
                         rsRetVal (*pConsumer)(void *, batch_t *, wti_t *)) {
    DEFiRet;
    qqueue_t *pThis;
    const uchar *const workDir = glblGetWorkDirRaw(ourConf);

    assert(ppThis != NULL);
    assert(pConsumer != NULL);
    assert(iWorkerThreads > 0);

    CHKmalloc(pThis = (qqueue_t *)calloc(1, sizeof(qqueue_t)));

    /* we have an object, so let's fill the properties */
    objConstructSetObjInfo(pThis);

    if (workDir != NULL) {
        if ((pThis->pszSpoolDir = ustrdup(workDir)) == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        pThis->lenSpoolDir = ustrlen(pThis->pszSpoolDir);
    }
    /* set some water marks so that we have useful defaults if none are set specifically */
    pThis->iFullDlyMrk = -1;
    pThis->iLightDlyMrk = -1;
    pThis->iMaxFileSize = 1024 * 1024; /* default is 1 MiB */
    pThis->iQueueSize = 0;
    pThis->nLogDeq = 0;
    pThis->useCryprov = 0;
    pThis->takeFlowCtlFromMsg = 0;
    pThis->iMaxQueueSize = iMaxQueueSize;
    pThis->pConsumer = pConsumer;
    pThis->iDeqtWinToHr = 25; /* disable time-windowed dequeuing by default */
    pThis->iDeqBatchSize = 8; /* conservative default, should still provide good performance */
    pThis->iMinDeqBatchSize = 0; /* conservative default, should still provide good performance */
    pThis->isRunning = 0;
    pThis->onCorruption = QUEUE_ON_CORRUPTION_SAFE_MODE;
    pThis->diskQueueType = QDA_ENGINE_AUTO;
    pThis->effectiveDiskQueueType = QDA_ENGINE_AUTO;
    pThis->diskQueueIdleTimeout = 60000;

    pThis->pszFilePrefix = NULL;
    pThis->qType = qType;


    INIT_ATOMIC_HELPER_MUT(pThis->mutQueueSize);
    INIT_ATOMIC_HELPER_MUT(pThis->mutLogDeq);
    INIT_ATOMIC_HELPER_MUT(pThis->mutShutdownImmediate);
    STATSCOUNTER_INIT(pThis->ctrMutexContention, pThis->mutCtrMutexContention);
    STATSCOUNTER_INIT(pThis->ctrMutexWaitNs, pThis->mutCtrMutexWaitNs);
    CHKiRet(qqueueSetiNumWorkerThreads(pThis, iWorkerThreads));

finalize_it:
    OBJCONSTRUCT_CHECK_SUCCESS_AND_CLEANUP
    RETiRet;
}


/* set default inside queue object suitable for action queues.
 * This shall be called directly after queue construction. This functions has
 * been added in support of the new v6 config system. It expect properly pre-initialized
 * objects, but we need to differentiate between ruleset main and action queues.
 * In order to avoid unnecessary complexity, we provide the necessary defaults
 * via specific function calls.
 */
void qqueueSetDefaultsActionQueue(qqueue_t *pThis) {
    pThis->qType = QUEUETYPE_DIRECT; /* type of the main message queue above */
    pThis->iMaxQueueSize = 1000; /* size of the main message queue above */
    pThis->iDeqBatchSize = 128; /* default batch size */
    pThis->iMinDeqBatchSize = 0;
    pThis->toMinDeqBatchSize = 1000;
    pThis->iHighWtrMrk = -1; /* high water mark for disk-assisted queues */
    pThis->iLowWtrMrk = -1; /* low water mark for disk-assisted queues */
    pThis->iDiscardMrk = -1; /* begin to discard messages */
    pThis->iDiscardSeverity = 8; /* turn off */
    pThis->iNumWorkerThreads = 1; /* number of worker threads for the mm queue above */
    pThis->iMaxFileSize = 1024 * 1024;
    pThis->iPersistUpdCnt = 0; /* persist queue info every n updates */
    pThis->bSyncQueueFiles = 0;
    pThis->toQShutdown = loadConf->globals.actq_dflt_toQShutdown; /* queue shutdown */
    pThis->toActShutdown = loadConf->globals.actq_dflt_toActShutdown; /* action shutdown (in phase 2) */
    pThis->toEnq = loadConf->globals.actq_dflt_toEnq; /* timeout for queue enque */
    pThis->toWrkShutdown = loadConf->globals.actq_dflt_toWrkShutdown; /* timeout for worker thread shutdown */
    pThis->iMinMsgsPerWrkr = -1; /* minimum messages per worker needed to start a new one */
    pThis->bSaveOnShutdown = 1; /* save queue on shutdown (when DA enabled)? */
    pThis->sizeOnDiskMax = 0; /* unlimited */
    pThis->iDeqSlowdown = 0;
    pThis->iDeqtWinFromHr = 0;
    pThis->iDeqtWinToHr = 25; /* disable time-windowed dequeuing by default */
    pThis->iSmpInterval = 0; /* disable sampling */
    pThis->bMutexContentionStats = 0;
    pThis->onCorruption = QUEUE_ON_CORRUPTION_SAFE_MODE;
}


/* set defaults inside queue object suitable for main/ruleset queues.
 * See queueSetDefaultsActionQueue() for more details and background.
 */
void qqueueSetDefaultsRulesetQueue(qqueue_t *pThis) {
    pThis->qType = QUEUETYPE_FIXED_ARRAY; /* type of the main message queue above */
    pThis->iMaxQueueSize = 50000; /* size of the main message queue above */
    pThis->iDeqBatchSize = 1024; /* default batch size */
    pThis->iMinDeqBatchSize = 0;
    pThis->toMinDeqBatchSize = 1000;
    pThis->iHighWtrMrk = -1; /* high water mark for disk-assisted queues */
    pThis->iLowWtrMrk = -1; /* low water mark for disk-assisted queues */
    pThis->iDiscardMrk = -1; /* begin to discard messages */
    pThis->iDiscardSeverity = 8; /* turn off */
    pThis->iNumWorkerThreads = 1; /* number of worker threads for the mm queue above */
    pThis->iMaxFileSize = 16 * 1024 * 1024;
    pThis->iPersistUpdCnt = 0; /* persist queue info every n updates */
    pThis->bSyncQueueFiles = 0;
    pThis->toQShutdown = ourConf->globals.ruleset_dflt_toQShutdown;
    pThis->toActShutdown = ourConf->globals.ruleset_dflt_toActShutdown;
    pThis->toEnq = ourConf->globals.ruleset_dflt_toEnq;
    pThis->toWrkShutdown = ourConf->globals.ruleset_dflt_toWrkShutdown;
    pThis->iMinMsgsPerWrkr = -1; /* minimum messages per worker needed to start a new one */
    pThis->bSaveOnShutdown = 1; /* save queue on shutdown (when DA enabled)? */
    pThis->sizeOnDiskMax = 0; /* unlimited */
    pThis->iDeqSlowdown = 0;
    pThis->iDeqtWinFromHr = 0;
    pThis->iDeqtWinToHr = 25; /* disable time-windowed dequeuing by default */
    pThis->iSmpInterval = 0; /* disable sampling */
    pThis->bMutexContentionStats = 0;
    pThis->onCorruption = QUEUE_ON_CORRUPTION_SAFE_MODE;
}


/* This function checks if the provided message shall be discarded and does so, if needed.
 * In DA mode, we do not discard any messages as we assume the disk subsystem is fast enough to
 * provide real-time creation of spool files.
 * Note: cached copies of iQueueSize is provided so that no mutex locks are required.
 * The caller must have obtained them while the mutex was locked. Of course, these values may no
 * longer be current, but that is OK for the discard check. At worst, the message is either processed
 * or discarded when it should not have been. As discarding is in itself somewhat racy and erratic,
 * that is no problems for us. This function MUST NOT lock the queue mutex, it could result in
 * deadlocks!
 * If the message is discarded, it can no longer be processed by the caller. So be sure to check
 * the return state!
 * rgerhards, 2008-01-24
 */
static int qqueueChkDiscardMsg(qqueue_t *pThis, int iQueueSize, smsg_t *pMsg) {
    DEFiRet;
    rsRetVal iRetLocal;
    int iSeverity;

    ISOBJ_TYPE_assert(pThis, qqueue);

    if (pThis->iDiscardMrk > 0 && iQueueSize >= pThis->iDiscardMrk) {
        iRetLocal = MsgGetSeverity(pMsg, &iSeverity);
        if (iRetLocal == RS_RET_OK && iSeverity >= pThis->iDiscardSeverity) {
            DBGOPRINT((obj_t *)pThis, "queue nearly full (%d entries), discarded severity %d message\n", iQueueSize,
                      iSeverity);
            STATSCOUNTER_INC(pThis->ctrNFDscrd, pThis->mutCtrNFDscrd);
            msgDestruct(&pMsg);
            ABORT_FINALIZE(RS_RET_QUEUE_FULL);
        } else {
            DBGOPRINT((obj_t *)pThis,
                      "queue nearly full (%d entries), but could not drop msg "
                      "(iRet: %d, severity %d)\n",
                      iQueueSize, iRetLocal, iSeverity);
        }
    }

finalize_it:
    RETiRet;
}

static rsRetVal handleReadSeekError(rsRetVal seekRet,
                                    qqueue_t *pThis,
                                    const char *streamName,
                                    sbool *const pReadSeekFailed) {
    assert(pReadSeekFailed != NULL);
    if (seekRet == RS_RET_OK) {
        return RS_RET_OK;
    }
    if (seekRet == RS_RET_FILE_NOT_FOUND) {
        LogError(0, seekRet, "%s: disk queue %s file missing on startup", obj.GetName((obj_t *)pThis), streamName);
        *pReadSeekFailed = 1;
        return RS_RET_OK;
    }
    return seekRet;
}

static void alignReadDeqToWrite(qqueue_t *pThis) {
    if (pThis->tVars.disk.pReadDeq == NULL || pThis->tVars.disk.pWrite == NULL) {
        return;
    }
    const rsRetVal syncRet = strm.Sync(pThis->tVars.disk.pReadDeq, pThis->tVars.disk.pWrite);
    if (syncRet != RS_RET_OK) {
        LogError(0, syncRet, "%s: could not sync disk queue read pointer to write pointer",
                 obj.GetName((obj_t *)pThis));
    }
    const rsRetVal seekRet = strm.SeekCurrOffs(pThis->tVars.disk.pReadDeq);
    if (seekRet != RS_RET_OK) {
        LogError(0, seekRet, "%s: could not seek disk queue read pointer after startup recovery hint",
                 obj.GetName((obj_t *)pThis));
    }
}

static void recoverFromInvalidQi(qqueue_t *pThis, const int wr_fd, const int64_t wr_offs) {
    pThis->tVars.disk.runtimeCorruptionSkip = 0;

    if (pThis->tVars.disk.pReadDel == NULL || pThis->tVars.disk.pWrite == NULL || wr_fd < 0 || wr_offs < 0) {
        return;
    }

    off64_t bytesDel = 0;
    int reset_done = 0;
    const unsigned int del_fnum = pThis->tVars.disk.pReadDel->iCurrFNum;
    if (del_fnum > (unsigned int)wr_fd) {
        LogError(0, RS_RET_ERR,
                 "%s: invalid .qi file; delete stream ahead of write stream "
                 "(del_fnum=%u, wr_fd=%d); keeping delete pointer",
                 obj.GetName((obj_t *)pThis), del_fnum, wr_fd);
        reset_done = 1;
    } else {
        const rsRetVal delRet = strmMultiFileSeek(pThis->tVars.disk.pReadDel, (unsigned int)wr_fd, wr_offs, &bytesDel);
        if (delRet != RS_RET_OK) {
            LogError(0, delRet, "%s: could not reset disk queue pointers after invalid .qi file",
                     obj.GetName((obj_t *)pThis));
        } else {
            if (bytesDel != 0) {
                if (pThis->tVars.disk.sizeOnDisk >= bytesDel) {
                    pThis->tVars.disk.sizeOnDisk -= bytesDel;
                } else {
                    pThis->tVars.disk.sizeOnDisk = 0;
                }
            }
            reset_done = 1;
        }
    }
    if (reset_done && pThis->tVars.disk.pReadDeq != NULL) {
        const rsRetVal syncRet = strm.Sync(pThis->tVars.disk.pReadDeq, pThis->tVars.disk.pReadDel);
        if (syncRet != RS_RET_OK) {
            LogError(0, syncRet, "%s: could not sync disk queue read pointer after invalid .qi file",
                     obj.GetName((obj_t *)pThis));
        }
        /* Non-fatal: queue already reset; keep draining if seek fails. */
        const rsRetVal seekRet = strm.SeekCurrOffs(pThis->tVars.disk.pReadDeq);
        if (seekRet != RS_RET_OK) {
            LogError(0, seekRet, "%s: could not seek disk queue read pointer after invalid .qi file",
                     obj.GetName((obj_t *)pThis));
        }
    }
    if (reset_done) {
        const rsRetVal persistRet = qqueuePersist(pThis, QUEUE_CHECKPOINT);
        if (persistRet != RS_RET_OK) {
            LogError(0, persistRet, "%s: could not persist disk queue after invalid .qi reset",
                     obj.GetName((obj_t *)pThis));
        } else {
            pThis->iUpdsSincePersist = 0;
        }
    }
}


/* Finally remove n elements from the queue store.
 */
static rsRetVal ATTR_NONNULL(1) DoDeleteBatchFromQStore(qqueue_t *const pThis, const int nElem) {
    int i;
    off64_t bytesDel = 0; /* keep CLANG static analyzer happy */
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);

    /* now send delete request to storage driver */
    if (pThis->qType == QUEUETYPE_DISK) {
        strmMultiFileSeek(pThis->tVars.disk.pReadDel, pThis->tVars.disk.deqFileNumOut, pThis->tVars.disk.deqOffs,
                          &bytesDel);
        /* We need to correct the on-disk file size. This time it is a bit tricky:
         * we free disk space only upon file deletion. So we need to keep track of what we
         * have read until we get an out-offset that is lower than the in-offset (which
         * indicates file change). Then, we can subtract the whole thing from the on-disk
         * size. -- rgerhards, 2008-01-30
         */
        if (bytesDel != 0) {
            if (pThis->tVars.disk.sizeOnDisk >= bytesDel) {
                pThis->tVars.disk.sizeOnDisk -= bytesDel;
            } else {
                pThis->tVars.disk.sizeOnDisk = 0;
            }
            DBGOPRINT((obj_t *)pThis,
                      "doDeleteBatch: a %lld octet file has been deleted, now %lld "
                      "octets disk space used\n",
                      (long long)bytesDel, pThis->tVars.disk.sizeOnDisk);
            /* awake possibly waiting enq process */
            pthread_cond_signal(&pThis->notFull); /* we hold the mutex while we are in here! */
        }
    } else if (pThis->qType == QUEUETYPE_FIXED_ARRAY) {
        /* RAM retirement releases completed counts, not the original slots of
         * this batch: another worker may finish an older batch later. Batch
         * pointers own the messages independently of these reusable slots.
         * Subtraction avoids overflowing head + nElem near INT_MAX. */
        const int remaining = pThis->iMaxQueueSize - pThis->tVars.farray.head;
        assert(nElem >= 0 && nElem <= pThis->iMaxQueueSize);
        pThis->tVars.farray.head = nElem >= remaining ? nElem - remaining : pThis->tVars.farray.head + nElem;
    } else { /* linked-list memory queue */
        for (i = 0; i < nElem; ++i) {
            pThis->qDel(pThis);
        }
    }

    /* iQueueSize is not decremented by qDel(), so we need to do it ourselves */
    ATOMIC_SUB(&pThis->iQueueSize, nElem, &pThis->mutQueueSize);
#ifdef ENABLE_IMDIAG
    #ifdef HAVE_ATOMIC_BUILTINS
    /* mutex is never used due to conditional compilation */
    ATOMIC_SUB(&iOverallQueueSize, nElem, &NULL);
    #else
    iOverallQueueSize -= nElem; /* racy, but we can't wait for a mutex! */
    #endif
#endif
    ATOMIC_SUB(&pThis->nLogDeq, nElem, &pThis->mutLogDeq);
    DBGPRINTF("doDeleteBatch: delete batch from store, new sizes: log %d, phys %d\n", getLogicalQueueSize(pThis),
              getPhysicalQueueSize(pThis));
    ++pThis->deqIDDel; /* one more batch dequeued */

    if ((pThis->qType == QUEUETYPE_DISK) && (bytesDel != 0)) {
        qqueuePersist(pThis, QUEUE_CHECKPOINT); /* robustness persist .qi file */
    }

    RETiRet;
}


typedef enum tdlPhase_e { TDL_EMPTY, TDL_PROCESS_HEAD, TDL_QUEUE } tdlPhase_t;

/**
 * Remove messages from the physical queue store that are fully processed.
 *
 * Deletion proceeds through a small state machine governed by the
 * to-delete list:
 * - TDL_EMPTY:  list is empty, delete the current batch directly.
 * - TDL_PROCESS_HEAD:  pending head elements are removed first, then the
 *   current batch.
 * - TDL_QUEUE:  current batch cannot be deleted and is queued for later.
 *
 * RAM batches release completed counts, including out-of-order worker
 * completions when the list is empty. Do not turn this into an ordered RAM
 * retirement frontier: workers already own independent message references.
 */
static rsRetVal DeleteBatchFromQStore(qqueue_t *pThis, batch_t *pBatch) {
    toDeleteLst_t *pTdl;
    qDeqID nextID;
    tdlPhase_t phase;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    assert(pBatch != NULL);

    dbgprintf("rger: deleteBatchFromQStore, nElem %d\n", (int)pBatch->nElem);
    pTdl = tdlPeek(pThis); /* get current head element */

    if (pTdl == NULL) {
        phase = TDL_EMPTY;
    } else if (pBatch->deqID == pThis->deqIDDel) {
        phase = TDL_PROCESS_HEAD;
    } else {
        phase = TDL_QUEUE;
    }

    switch (phase) {
        case TDL_EMPTY:
            DoDeleteBatchFromQStore(pThis, pBatch->nElemDeq);
            break;

        case TDL_PROCESS_HEAD:
            nextID = pThis->deqIDDel;
            while ((pTdl = tdlPeek(pThis)) != NULL && pTdl->deqID == nextID) {
                DoDeleteBatchFromQStore(pThis, pTdl->nElemDeq);
                tdlPop(pThis);
                ++nextID;
            }
            assert(pThis->deqIDDel == nextID);
            /* old entries deleted, now delete current ones... */
            DoDeleteBatchFromQStore(pThis, pBatch->nElemDeq);
            break;

        case TDL_QUEUE:
            /* cannot delete, insert into to-delete list */
            DBGPRINTF("not at head of to-delete list, enqueue %d\n", (int)pBatch->deqID);
            CHKiRet(tdlAdd(pThis, pBatch->deqID, pBatch->nElemDeq));
            break;

        default:
            /* all phases should be handled above. */
            assert(0 && "unhandled tdlPhase_t");
            break;
    }

finalize_it:
    RETiRet;
}


/* Concurrency & Locking: completed batch references move to a worker-owned
 * buffer under the queue mutex, after store commit. The buffer holds at most
 * one batch and is drained without the queue mutex and with cancellation
 * disabled. No other worker, producer, or shutdown path accesses this buffer.
 */
static void qqueueDrainDeferred(wti_t *const pWti) {
    for (int i = 0; i < pWti->n_deferred_msgs; ++i) {
        msgDestruct(&pWti->p_deferred_msgs[i]);
    }
    pWti->n_deferred_msgs = 0;
}

/* Exceptional idle/minbatch/cleanup path: never retain a completed batch
 * across a condition wait. Recheck queue predicates after reacquiring. */
static void qqueueDrainDeferredLocked(qqueue_t *const pThis, wti_t *const pWti) {
    if (pWti->n_deferred_msgs != 0) {
        d_pthread_mutex_unlock(pThis->mut);
        qqueueDrainDeferred(pWti);
        qqueueLock(pThis);
    }
}

static void qqueueDeferBatch(wti_t *const pWti) {
    batch_t *const pBatch = &pWti->batch;
    assert(pWti->n_deferred_msgs == 0);
    assert(pBatch->nElem <= pBatch->maxElem);
    for (int i = 0; i < pBatch->nElem; ++i) {
        pWti->p_deferred_msgs[i] = pBatch->pElem[i].pMsg;
        pBatch->pElem[i].pMsg = NULL;
    }
    pWti->n_deferred_msgs = pBatch->nElem;
    pBatch->nElem = pBatch->nElemDeq = 0;
}

/* Local FixedArray completion returns retry references to capacity released
 * by this same batch, under the BE mutex. This is an internal ownership move:
 * it neither enters external admission nor waits/discards on a full queue. */
static rsRetVal qqueueCompleteLocalBackend(qqueue_t *const queue, wti_t *const worker) {
    batch_t *const batch = &worker->batch;
    if (batch->nElemDeq == 0) return RS_RET_OK;
    if (queue->qType != QUEUETYPE_FIXED_ARRAY || queue->toDeleteLst != NULL || batch->nElem != batch->nElemDeq ||
        worker->source_queue != queue)
        return RS_RET_INTERNAL_ERROR;
    const rsRetVal ret = DoDeleteBatchFromQStore(queue, batch->nElemDeq);
    if (ret != RS_RET_OK) return ret;
    uint64_t terminal = 0, discarded = 0;
    const int diskTransfer = worker->pWtp == queue->pWtpDA;
    assert(worker->n_deferred_msgs == 0);
    for (int i = 0; i < batch->nElem; ++i) {
        smsg_t *const message = batch->pElem[i].pMsg;
        if (batch->eltState[i] == BATCH_STATE_RDY || batch->eltState[i] == BATCH_STATE_SUB) {
            assert(queue->iQueueSize < queue->iMaxQueueSize);
            /* FixedArray add does not allocate or fail. Sampling is rejected
             * by local activation, and no external producer holds this mutex. */
            qqueueAdd(queue, message);
        } else {
            worker->p_deferred_msgs[worker->n_deferred_msgs++] = message;
            if (!diskTransfer || batch->eltState[i] != BATCH_STATE_COMM) {
                ++terminal;
                if (batch->eltState[i] == BATCH_STATE_DISC) ++discarded;
            }
        }
        batch->pElem[i].pMsg = NULL;
    }
    batch->nElem = batch->nElemDeq = 0;
    qqueueLocalBackendTerminal(queue, terminal, discarded);
    pthread_cond_broadcast(&queue->notFull);
    pthread_cond_broadcast(&queue->belowLightDlyWtrMrk);
    qqueueLocalBackendWakeSpace(queue);
    qqueueLocalWakeBackendHelpers(queue);
    if (getLogicalQueueSize(queue) != 0 && !qqueueLocalIsClosed(queue)) qqueueAdviseMaxWorkers(queue);
    return qqueueClearWtiSource(queue, worker);
}

/* Explicit local-family authority, independent of the helper's immutable FE
 * pool identity. Both operations require the BE mutex and exclusive WTI
 * ownership with cancellation disabled; no maintenance accesses it before join. */
rsRetVal qqueueLocalTryBorrowBackend(qqueue_t *const owner, wti_t *const worker, const unsigned limit) {
    batch_t *const batch = &worker->batch;
    if (!qqueueLocalBorrowWorker(owner, worker) || owner->qType != QUEUETYPE_FIXED_ARRAY ||
        worker->source_queue != NULL || batch->nElem != 0 || batch->nElemDeq != 0 || batch->storeData != NULL ||
        worker->n_deferred_msgs != 0 || limit > (unsigned)batch->maxElem)
        return RS_RET_INTERNAL_ERROR;
    if (owner->localDAActive) return RS_RET_IDLE;
    unsigned count = (unsigned)getLogicalQueueSize(owner);
    if (count > limit) count = limit;
    if (count == 0) return RS_RET_IDLE;
    worker->source_queue = owner;
    worker->logical_owner = owner;
    for (unsigned i = 0; i < count; ++i) {
        qDeqFixedArray(owner, &batch->pElem[i].pMsg);
        batch->eltState[i] = BATCH_STATE_RDY;
    }
    qqueueAddLogDeq(owner, (int)count);
    batch->nElem = batch->nElemDeq = (int)count;
    batch->deqID = getNextDeqID(owner);
    qqueueLocalBackendAcquired(owner, count);
    return RS_RET_OK;
}

rsRetVal qqueueLocalCompleteBorrowedBackend(qqueue_t *const owner, wti_t *const worker) {
    if (!qqueueLocalBorrowWorker(owner, worker) || worker->source_queue != owner || worker->logical_owner != owner)
        return RS_RET_INTERNAL_ERROR;
    return qqueueCompleteLocalBackend(owner, worker);
}

/* Delete a batch of processed user objects from the queue, which includes
 * destructing the objects themself. Any entries not marked as finally
 * processed are enqueued again. The new enqueue is necessary because we have a
 * rgerhards, 2009-05-13
 */
static rsRetVal DeleteProcessedBatch(qqueue_t *pThis, wti_t *pWti) {
    batch_t *const pBatch = &pWti->batch;
    const int hadResponsibility = qqueueLeaseHasResponsibility(pBatch->nElem, pBatch->nElemDeq, pBatch->storeData);
    int i;
    smsg_t *pMsg;
    int nEnqueued = 0;
    rsRetVal localRet;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    assert(pBatch != NULL);
    if (pThis->local != NULL) return qqueueCompleteLocalBackend(pThis, pWti);

    qqueue_t *const diskOwner = pThis->pqParent != NULL && pThis->pqParent->local != NULL ? pThis->pqParent : NULL;
    /* Store-only corrupt records have no decoded batch element. They still
     * belong to the recovered inventory and retire with this physical lease. */
    uint64_t diskTerminal = pBatch->nElemDeq > pBatch->nElem ? (uint64_t)(pBatch->nElemDeq - pBatch->nElem) : 0;
    uint64_t diskDiscarded = diskTerminal;
    if (diskOwner != NULL) {
        for (i = 0; i < pBatch->nElem; ++i) {
            if (pBatch->eltState[i] == BATCH_STATE_COMM || pBatch->eltState[i] == BATCH_STATE_DISC) ++diskTerminal;
            if (pBatch->eltState[i] == BATCH_STATE_DISC) ++diskDiscarded;
        }
    }
    if (pThis->qCompleteBatch != NULL && pBatch->storeData != NULL) {
        int committed = 0;
        int retried = 0;
        iRet = pThis->qCompleteBatch(pThis, pBatch, &committed, &retried);
        if (iRet != RS_RET_OK) RETiRet;
        /* committed is the physical-record count. It intentionally includes
         * salvaged corrupt records because recovery included those records in
         * iQueueSize; retried counts only decoded messages appended again. */
        if (committed > retried) {
            const int removed = committed - retried;
            ATOMIC_SUB(&pThis->iQueueSize, removed, &pThis->mutQueueSize);
            qqueueSubtractOverallQueueSize(removed);
            /* A producer may be waiting on either the logical queue limit or
             * segmented-store disk space released by this completion. */
            pthread_cond_signal(&pThis->notFull);
        } else if (retried > committed) {
            const int added = retried - committed;
            qqueueAddPhysicalQueueSize(pThis, added);
            qqueueAddOverallQueueSize(added);
        }
        ATOMIC_SUB(&pThis->nLogDeq, committed, &pThis->mutLogDeq);
        if (diskOwner != NULL) qqueueLocalDiskTerminal(diskOwner, diskTerminal, diskDiscarded);
        qqueueDeferBatch(pWti);
        pBatch->storeData = NULL;
        if (hadResponsibility) {
            iRet = qqueueClearWtiSource(pThis, pWti);
            if (iRet != RS_RET_OK) RETiRet;
        }
        RETiRet;
    }

    for (i = 0; i < pBatch->nElem; ++i) {
        pMsg = pBatch->pElem[i].pMsg;
        DBGPRINTF("DeleteProcessedBatch: etry %d state %d\n", i, pBatch->eltState[i]);
        if (pBatch->eltState[i] == BATCH_STATE_RDY || pBatch->eltState[i] == BATCH_STATE_SUB) {
            localRet = doEnqSingleObjContext(pThis, eFLOWCTL_NO_DELAY, MsgAddRef(pMsg), pThis->localGraphConf != NULL);
            ++nEnqueued;
            if (localRet != RS_RET_OK) {
                ++diskTerminal;
                ++diskDiscarded;
                DBGPRINTF(
                    "DeleteProcessedBatch: error %d re-enqueuing unprocessed "
                    "data element - discarded\n",
                    localRet);
            }
        }
    }

    DBGPRINTF("DeleteProcessedBatch: we deleted %d objects and enqueued %d objects\n", i - nEnqueued, nEnqueued);

    if (nEnqueued > 0) qqueueChkPersist(pThis, nEnqueued);

    iRet = DeleteBatchFromQStore(pThis, pBatch);
    if (iRet == RS_RET_OK && diskOwner != NULL) qqueueLocalDiskTerminal(diskOwner, diskTerminal, diskDiscarded);

    qqueueDeferBatch(pWti);
    if (iRet == RS_RET_OK && hadResponsibility) iRet = qqueueClearWtiSource(pThis, pWti);

    RETiRet;
}


/* dequeue as many user pointers as are available, until we hit the configured
 * upper limit of pointers. Note that this function also deletes all processed
 * objects from the previous batch. However, it is perfectly valid that the
 * previous batch contained NO objects at all. For example, this happens
 * immediately after system startup or when a queue was exhausted and the queue
 * worker needed to wait for new data.
 * This must only be called when the queue mutex is LOOKED, otherwise serious
 * malfunction will happen.
 */
static rsRetVal ATTR_NONNULL() DequeueConsumableElements(qqueue_t *const pThis,
                                                         wti_t *const pWti,
                                                         int *const piRemainingQueueSize,
                                                         int *const pSkippedMsgs) {
    int nDequeued;
    int nDiscarded;
    int nDeleted;
    int iQueueSize;
    int keep_running = 1;
    int deferredDiskCorruption = 0;
    rsRetVal pendingCorruptRet = RS_RET_OK;
    struct timespec timeout;
    smsg_t *pMsg;
    rsRetVal localRet;
    DEFiRet;
    int diskPhysicalBefore = -1;

    nDeleted = pWti->batch.nElemDeq;
    localRet = DeleteProcessedBatch(pThis, pWti);
    if (pThis->qCompleteBatch != NULL || pThis->local != NULL) CHKiRet(localRet);
    if (pThis->qType == QUEUETYPE_DISK && pThis->pqParent != NULL && pThis->pqParent->local != NULL)
        diskPhysicalBefore = getPhysicalQueueSize(pThis);
    /* The previous lease was cleared only after retirement. Bind before every
     * outcome of the next acquisition, including idle and store errors. */
    CHKiRet(qqueueBindWtiSource(pThis, pWti));

    nDequeued = nDiscarded = 0;
    if (pThis->qDeqBatch != NULL) {
        localRet = pThis->qDeqBatch(pThis, &pWti->batch, pThis->iDeqBatchSize, pSkippedMsgs);
        if (localRet == RS_RET_NO_DATA) {
            /* With a returned batch, framed corrupt records are included in
             * nElemDeq and retired by qCompleteBatch. Only an all-corrupt
             * result has no batch context, so retire those records here. */
            if (*pSkippedMsgs > 0) {
                ATOMIC_SUB(&pThis->iQueueSize, *pSkippedMsgs, &pThis->mutQueueSize);
                qqueueSubtractOverallQueueSize(*pSkippedMsgs);
                if (pThis->pqParent != NULL)
                    qqueueLocalDiskTerminal(pThis->pqParent, (uint64_t)*pSkippedMsgs, (uint64_t)*pSkippedMsgs);
            }
            pWti->batch.nElem = pWti->batch.nElemDeq = 0;
            *piRemainingQueueSize = getLogicalQueueSize(pThis);
            iRet = RS_RET_OK;
            FINALIZE;
        }
        CHKiRet(localRet);
        for (int i = 0; i < pWti->batch.nElem; ++i) {
            localRet = qqueueChkDiscardMsg(pThis, pThis->iQueueSize, pWti->batch.pElem[i].pMsg);
            if (localRet == RS_RET_QUEUE_FULL) {
                pWti->batch.eltState[i] = BATCH_STATE_DISC;
                ++nDiscarded;
            } else {
                CHKiRet(localRet);
            }
        }
        qqueueAddLogDeq(pThis, pWti->batch.nElemDeq);
        pWti->batch.deqID = getNextDeqID(pThis);
        *piRemainingQueueSize = getLogicalQueueSize(pThis);
        FINALIZE;
    }
    if (pThis->qType == QUEUETYPE_DISK) {
        pThis->tVars.disk.deqFileNumIn = strmGetCurrFileNum(pThis->tVars.disk.pReadDeq);
        pThis->tVars.disk.runtimeCorruptionSkip = 0;
        if (pThis->tVars.disk.pendingCorruptRet != RS_RET_OK) {
            pendingCorruptRet = pThis->tVars.disk.pendingCorruptRet;
            pThis->tVars.disk.pendingCorruptRet = RS_RET_OK;
        }
    }

    /* work-around clang static analyzer false positive, we need a const value */
    const int iMinDeqBatchSize = pThis->iMinDeqBatchSize;
    if (iMinDeqBatchSize > 0) {
        timeoutComp(&timeout, pThis->toMinDeqBatchSize); /* get absolute timeout */
    }

    while ((iQueueSize = getLogicalQueueSize(pThis)) > 0 && nDequeued < pThis->iDeqBatchSize) {
        int rd_fd = -1;
        int64_t rd_offs = 0;
        int wr_fd = -1;
        int64_t wr_offs = 0;
        if (pThis->tVars.disk.pReadDeq != NULL) {
            rd_fd = strmGetCurrFileNum(pThis->tVars.disk.pReadDeq);
            rd_offs = pThis->tVars.disk.pReadDeq->iCurrOffs;
        }
        if (pThis->tVars.disk.pWrite != NULL) {
            wr_fd = strmGetCurrFileNum(pThis->tVars.disk.pWrite);
            wr_offs = pThis->tVars.disk.pWrite->iCurrOffs;
        }
        pMsg = NULL;
        if (pendingCorruptRet != RS_RET_OK) {
            localRet = pendingCorruptRet;
            pendingCorruptRet = RS_RET_OK;
        } else {
            if (rd_fd != -1 && rd_fd == wr_fd && rd_offs == wr_offs) {
                DBGPRINTF(
                    "problem on disk queue '%s': "
                    //"queue size log %d, phys %d, but rd_fd=wr_rd=%d and offs=%lld\n",
                    "queue size log %d, phys %d, but rd_fd=wr_rd=%d and offs=%" PRId64 "\n",
                    obj.GetName((obj_t *)pThis), iQueueSize, pThis->iQueueSize, rd_fd, rd_offs);
                if (pThis->onCorruption == QUEUE_ON_CORRUPTION_SAFE_MODE && iQueueSize > 1) {
                    LogMsg(0, RS_RET_ERR, LOG_ALERT,
                           "%s: disk queue corruption reached the write pointer with %d logical records remaining; "
                           "quarantining unread tail",
                           obj.GetName((obj_t *)pThis), iQueueSize);
                    pThis->tVars.disk.runtimeCorruptionSkip = 1;
                    *pSkippedMsgs += iQueueSize;
                    qqueueSubtractOverallQueueSize(iQueueSize);
                    pThis->iQueueSize -= iQueueSize;
                    CHKiRet(qqueueResetDiskQueueAfterCorruption(pThis));
                    pThis->tVars.disk.runtimeCorruptionSkip = 1;
                    nDiscarded = 0;
                    iQueueSize = 0;
                    break;
                }
                *pSkippedMsgs = iQueueSize;
                qqueueSubtractOverallQueueSize(iQueueSize - nDiscarded);
                pThis->iQueueSize -= iQueueSize;
                recoverFromInvalidQi(pThis, wr_fd, wr_offs);
                iQueueSize = 0;
                break;
            }

            localRet = pThis->qDeq(pThis, &pMsg);
            if (localRet == RS_RET_FILE_NOT_FOUND) {
                DBGPRINTF(
                    "fatal error on disk queue '%s': file '%s' "
                    "not found, queue size said to be %d",
                    obj.GetName((obj_t *)pThis), "...", iQueueSize);
                if (pThis->qType == QUEUETYPE_DISK) {
                    if (pThis->onCorruption == QUEUE_ON_CORRUPTION_SAFE_MODE && iQueueSize > 1) {
                        LogMsg(0, localRet, LOG_ALERT,
                               "%s: disk queue corruption reached a missing file with %d logical records remaining; "
                               "quarantining unread tail",
                               obj.GetName((obj_t *)pThis), iQueueSize);
                        pThis->tVars.disk.runtimeCorruptionSkip = 1;
                        *pSkippedMsgs += iQueueSize;
                        qqueueSubtractOverallQueueSize(iQueueSize);
                        pThis->iQueueSize -= iQueueSize;
                        CHKiRet(qqueueResetDiskQueueAfterCorruption(pThis));
                        pThis->tVars.disk.runtimeCorruptionSkip = 1;
                        nDiscarded = 0;
                        iQueueSize = 0;
                        break;
                    }
                    *pSkippedMsgs = iQueueSize;
                    qqueueSubtractOverallQueueSize(iQueueSize - nDiscarded);
                    pThis->iQueueSize -= iQueueSize;
                    recoverFromInvalidQi(pThis, wr_fd, wr_offs);
                    iQueueSize = 0;
                    break;
                }
            }
            if (localRet != RS_RET_OK && pThis->qType == QUEUETYPE_DISK && pThis->tVars.disk.pReadDeq != NULL &&
                pThis->tVars.disk.pWrite != NULL &&
                strmGetCurrFileNum(pThis->tVars.disk.pReadDeq) == strmGetCurrFileNum(pThis->tVars.disk.pWrite) &&
                pThis->tVars.disk.pReadDeq->iCurrOffs >= pThis->tVars.disk.pWrite->iCurrOffs) {
                if (pThis->onCorruption == QUEUE_ON_CORRUPTION_SAFE_MODE && iQueueSize > 1) {
                    LogMsg(0, localRet, LOG_ALERT,
                           "%s: disk queue corruption reached the write pointer with %d logical records remaining; "
                           "quarantining unread tail",
                           obj.GetName((obj_t *)pThis), iQueueSize);
                    pThis->tVars.disk.runtimeCorruptionSkip = 1;
                    *pSkippedMsgs += iQueueSize;
                    qqueueSubtractOverallQueueSize(iQueueSize);
                    pThis->iQueueSize -= iQueueSize;
                    CHKiRet(qqueueResetDiskQueueAfterCorruption(pThis));
                    pThis->tVars.disk.runtimeCorruptionSkip = 1;
                    nDiscarded = 0;
                    iQueueSize = 0;
                    break;
                }
                *pSkippedMsgs = iQueueSize;
                qqueueSubtractOverallQueueSize(iQueueSize - nDiscarded);
                pThis->iQueueSize -= iQueueSize;
                recoverFromInvalidQi(pThis, strmGetCurrFileNum(pThis->tVars.disk.pWrite),
                                     pThis->tVars.disk.pWrite->iCurrOffs);
                iQueueSize = 0;
                break;
            }
        }
        if (localRet != RS_RET_OK && pThis->qType == QUEUETYPE_DISK &&
            pThis->onCorruption == QUEUE_ON_CORRUPTION_SAFE_MODE) {
            int nSkippedCorrupt = 0;
            if (nDequeued > 0) {
                LogMsg(0, localRet, LOG_WARNING,
                       "%s: disk queue corruption detected after %d valid records; deferring recovery until "
                       "the current batch is committed",
                       obj.GetName((obj_t *)pThis), nDequeued);
                deferredDiskCorruption = 1;
                pThis->tVars.disk.pendingCorruptRet = localRet;
                iRet = RS_RET_OK;
                break;
            }
            localRet = qDeqDiskRecoverAfterCorruption(pThis, &pMsg, localRet, &nSkippedCorrupt);
            if (localRet != RS_RET_OK && pThis->tVars.disk.pReadDeq != NULL && pThis->tVars.disk.pWrite != NULL &&
                strmGetCurrFileNum(pThis->tVars.disk.pReadDeq) == strmGetCurrFileNum(pThis->tVars.disk.pWrite) &&
                pThis->tVars.disk.pReadDeq->iCurrOffs >= pThis->tVars.disk.pWrite->iCurrOffs) {
                if (iQueueSize > 1) {
                    LogMsg(0, localRet, LOG_ALERT,
                           "%s: disk queue corruption reached the write pointer with %d logical records remaining; "
                           "quarantining unread tail",
                           obj.GetName((obj_t *)pThis), iQueueSize);
                    pThis->tVars.disk.runtimeCorruptionSkip = 1;
                    *pSkippedMsgs += iQueueSize;
                    qqueueSubtractOverallQueueSize(iQueueSize);
                    pThis->iQueueSize -= iQueueSize;
                    CHKiRet(qqueueResetDiskQueueAfterCorruption(pThis));
                    pThis->tVars.disk.runtimeCorruptionSkip = 1;
                    nDiscarded = 0;
                    iQueueSize = 0;
                    break;
                }
                *pSkippedMsgs = iQueueSize;
                qqueueSubtractOverallQueueSize(iQueueSize - nDiscarded);
                pThis->iQueueSize -= iQueueSize;
                recoverFromInvalidQi(pThis, strmGetCurrFileNum(pThis->tVars.disk.pWrite),
                                     pThis->tVars.disk.pWrite->iCurrOffs);
                iQueueSize = 0;
                break;
            }
            if (localRet == RS_RET_NO_DATA) {
                pThis->tVars.disk.runtimeCorruptionSkip = 1;
                *pSkippedMsgs += nSkippedCorrupt;
                qqueueSubtractOverallQueueSize(nSkippedCorrupt);
                iQueueSize = 0;
                break;
            }
            if (nSkippedCorrupt > 0) {
                pThis->tVars.disk.runtimeCorruptionSkip = 1;
                nDiscarded += nSkippedCorrupt;
                *pSkippedMsgs += nSkippedCorrupt;
            }
            if (localRet == RS_RET_OK) {
                qqueueAddLogDeq(pThis, nSkippedCorrupt + 1);
            }
        } else if (localRet == RS_RET_OK) {
            qqueueAddLogDeq(pThis, 1);
        }
        CHKiRet(localRet);
        if (pThis->qType == QUEUETYPE_DISK) {
            strm.GetCurrOffset(pThis->tVars.disk.pReadDeq, &pThis->tVars.disk.deqOffs);
            pThis->tVars.disk.deqFileNumOut = strmGetCurrFileNum(pThis->tVars.disk.pReadDeq);
        }

        /* check if we should discard this element */
        localRet = qqueueChkDiscardMsg(pThis, pThis->iQueueSize, pMsg);
        if (localRet == RS_RET_QUEUE_FULL) {
            ++nDiscarded;
            continue;
        } else if (localRet != RS_RET_OK) {
            ABORT_FINALIZE(localRet);
        }

        /* all well, use this element */
        pWti->batch.pElem[nDequeued].pMsg = pMsg;
        pWti->batch.eltState[nDequeued] = BATCH_STATE_RDY;
        ++nDequeued;
        if (nDequeued < iMinDeqBatchSize && getLogicalQueueSize(pThis) == 0) {
            qqueueDrainDeferredLocked(pThis, pWti);
            while (!qqueueIsShutdownImmediate(pThis) && keep_running && nDequeued < iMinDeqBatchSize &&
                   getLogicalQueueSize(pThis) == 0) {
                dbgprintf(
                    "%s minDeqBatchSize doing wait, batch is %d messages, "
                    "queue size %d\n",
                    obj.GetName((obj_t *)pThis), nDequeued, getLogicalQueueSize(pThis));
                if (wtiWaitNonEmpty(pWti, timeout) == 0) { /* timeout? */
                    DBGPRINTF("%s minDeqBatchSize timeout, batch is %d messages\n", obj.GetName((obj_t *)pThis),
                              nDequeued);
                    keep_running = 0;
                }
            }
        }
        if (keep_running) {
            keep_running = (getLogicalQueueSize(pThis) > 0) && (nDequeued < pThis->iDeqBatchSize);
        }
    }

    if (pThis->qType == QUEUETYPE_DISK && !deferredDiskCorruption) {
        strm.GetCurrOffset(pThis->tVars.disk.pReadDeq, &pThis->tVars.disk.deqOffs);
        pThis->tVars.disk.deqFileNumOut = strmGetCurrFileNum(pThis->tVars.disk.pReadDeq);
    }

    /* it is sufficient to persist only when the bulk of work is done */
    qqueueChkPersist(pThis, nDequeued + nDiscarded + nDeleted);

    /* If messages where DISCARDED, we need to substract them from the OverallQueueSize */
#ifdef ENABLE_IMDIAG
    #ifdef HAVE_ATOMIC_BUILTINS
    ATOMIC_SUB(&iOverallQueueSize, nDiscarded, &NULL);
    #else
    iOverallQueueSize -= nDiscarded; /* racy, but we can't wait for a mutex! */
    #endif
    DBGOPRINT((obj_t *)pThis, "dequeued %d discarded %d QueueSize %d consumable elements, szlog %d sz phys %d\n",
              nDequeued, nDiscarded, iOverallQueueSize, getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis));
#else
    DBGOPRINT((obj_t *)pThis, "dequeued %d discarded %d consumable elements, szlog %d sz phys %d\n", nDequeued,
              nDiscarded, getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis));
#endif

    if (pThis->local != NULL) qqueueLocalBackendAcquired(pThis, (uint64_t)nDequeued);
    pWti->batch.nElem = nDequeued;
    pWti->batch.nElemDeq = nDequeued + nDiscarded;
    pWti->batch.deqID = getNextDeqID(pThis);
    *piRemainingQueueSize = iQueueSize;
finalize_it:
    /* Classic corruption recovery can remove an unread tail immediately,
     * outside the returned batch lease. Observe that definite disposition
     * once; framed skipped records still in nElemDeq retire at completion. */
    if (diskPhysicalBefore >= 0 && getPhysicalQueueSize(pThis) < diskPhysicalBefore) {
        const uint64_t lost = (uint64_t)(diskPhysicalBefore - getPhysicalQueueSize(pThis));
        qqueueLocalDiskTerminal(pThis->pqParent, lost, lost);
    }
    RETiRet;
}


/* dequeue the queued object for the queue consumers.
 * rgerhards, 2008-10-21
 * I made a radical change - we now dequeue multiple elements, and store these objects in
 * an array of user pointers. We expect that this increases performance.
 * rgerhards, 2009-04-22
 */
static rsRetVal DequeueConsumable(qqueue_t *pThis, wti_t *pWti, int *const pSkippedMsgs) {
    DEFiRet;
    int iQueueSize = 0; /* keep the compiler happy... */

    *pSkippedMsgs = 0;
    /* dequeue element batch (still protected from mutex) */
    iRet = DequeueConsumableElements(pThis, pWti, &iQueueSize, pSkippedMsgs);
    if (*pSkippedMsgs > 0) {
        if (pThis->qType == QUEUETYPE_SEGMENTED_DISK) {
            LogMsg(0, RS_RET_ERR, LOG_ERR, "%s: segmentedDisk salvage skipped %d corrupt records",
                   obj.GetName((obj_t *)pThis), *pSkippedMsgs);
        } else if (pThis->qType == QUEUETYPE_DISK && pThis->tVars.disk.runtimeCorruptionSkip) {
            LogMsg(0, RS_RET_ERR, LOG_ERR, "%s: skipped %d messages from diskqueue during runtime corruption handling",
                   obj.GetName((obj_t *)pThis), *pSkippedMsgs);
        } else {
            LogError(0, RS_RET_ERR, "%s: lost %d messages from diskqueue (invalid .qi file)",
                     obj.GetName((obj_t *)pThis), *pSkippedMsgs);
        }
    }

    /* awake some flow-controlled sources if we can do this right now */
    /* TODO: this could be done better from a performance point of view -- do it only if
     * we have someone waiting for the condition (or only when we hit the watermark right
     * on the nail [exact value]) -- rgerhards, 2008-03-14
     * now that we dequeue batches of pointers, this is much less an issue...
     * rgerhards, 2009-04-22
     */
    if (iQueueSize < pThis->iFullDlyMrk / 2 || glbl.GetGlobalInputTermState() == 1) {
        pthread_cond_broadcast(&pThis->belowFullDlyWtrMrk);
    }

    if (iQueueSize < pThis->iLightDlyMrk / 2) {
        pthread_cond_broadcast(&pThis->belowLightDlyWtrMrk);
    }

    pthread_cond_signal(&pThis->notFull);
    /* WE ARE NO LONGER PROTECTED BY THE MUTEX */

    if (iRet != RS_RET_OK && iRet != RS_RET_DISCARDMSG) {
        LogError(0, iRet,
                 "%s: error dequeueing element - ignoring, "
                 "but strange things may happen",
                 obj.GetName((obj_t *)pThis));
    }

    RETiRet;
}


/* The rate limiter
 *
 * IMPORTANT: the rate-limiter MUST unlock and re-lock the queue when
 * it actually delays processing. Otherwise inputs are stalled.
 *
 * Here we may wait if a dequeue time window is defined or if we are
 * rate-limited. TODO: If we do so, we should also look into the
 * way new worker threads are spawned. Obviously, it doesn't make much
 * sense to spawn additional worker threads when none of them can do any
 * processing. However, it is deemed acceptable to allow this for an initial
 * implementation of the timeframe/rate limiting feature.
 * Please also note that these feature could also be implemented at the action
 * level. However, that would limit them to be used together with actions. We have
 * taken the broader approach, moving it right into the queue. This is even
 * necessary if we want to prevent spawning of multiple unnecessary worker
 * threads as described above. -- rgerhards, 2008-04-02
 *
 *
 * time window: tCurr is current time; tFrom is start time, tTo is end time (in mil 24h format).
 * We may have tFrom = 4, tTo = 10 --> run from 4 to 10 hrs. nice and happy
 * we may also have tFrom= 22, tTo = 4 -> run from 10pm to 4am, which is actually two
 *     windows: 0-4; 22-23:59
 * so when to run? Let's assume we have 3am
 *
 * if(tTo < tFrom) {
 * 	if(tCurr < tTo [3 < 4] || tCurr > tFrom [3 > 22])
 * 		do work
 * 	else
 * 		sleep for tFrom - tCurr "hours" [22 - 5 --> 17]
 * } else {
 * 	if(tCurr >= tFrom [3 >= 4] && tCurr < tTo [3 < 10])
 * 		do work
 * 	else
 * 		sleep for tTo - tCurr "hours" [4 - 3 --> 1]
 * }
 *
 * Bottom line: we need to check which type of window we have and need to adjust our
 * logic accordingly. Of course, sleep calculations need to be done up to the minute,
 * but you get the idea from the code above.
 */
static rsRetVal RateLimiter(qqueue_t *pThis) {
    DEFiRet;
    int iDelay;
    int iHrCurr;
    time_t tCurr;
    struct tm m;

    ISOBJ_TYPE_assert(pThis, qqueue);

    iDelay = 0;
    if (pThis->iDeqtWinToHr != 25) { /* 25 means disabled */
        /* time calls are expensive, so only do them when needed */
        datetime.GetTime(&tCurr);
        localtime_r(&tCurr, &m);
        iHrCurr = m.tm_hour;

        if (pThis->iDeqtWinToHr < pThis->iDeqtWinFromHr) {
            if (iHrCurr < pThis->iDeqtWinToHr || iHrCurr > pThis->iDeqtWinFromHr) {
                ; /* do not delay */
            } else {
                iDelay = (pThis->iDeqtWinFromHr - iHrCurr) * 3600;
                /* this time, we are already into the next hour, so we need
                 * to subtract our current minute and seconds.
                 */
                iDelay -= m.tm_min * 60;
                iDelay -= m.tm_sec;
            }
        } else {
            if (iHrCurr >= pThis->iDeqtWinFromHr && iHrCurr < pThis->iDeqtWinToHr) {
                ; /* do not delay */
            } else {
                if (iHrCurr < pThis->iDeqtWinFromHr) {
                    iDelay = (pThis->iDeqtWinFromHr - iHrCurr - 1) * 3600;
                    /* -1 as we are already in the hour */
                    iDelay += (60 - m.tm_min) * 60;
                    iDelay += 60 - m.tm_sec;
                } else {
                    iDelay = (24 - iHrCurr + pThis->iDeqtWinFromHr) * 3600;
                    /* this time, we are already into the next hour, so we need
                     * to subtract our current minute and seconds.
                     */
                    iDelay -= m.tm_min * 60;
                    iDelay -= m.tm_sec;
                }
            }
        }
    }

    if (iDelay > 0) {
        pthread_mutex_unlock(pThis->mut);
        DBGOPRINT((obj_t *)pThis, "outside dequeue time window, delaying %d seconds\n", iDelay);
        srSleep(iDelay, 0);
        qqueueLock(pThis);
    }

    RETiRet;
}


/* This dequeues the next batch. Note that this function must not be
 * cancelled, else it will leave back an inconsistent state.
 * rgerhards, 2009-05-20
 */
static rsRetVal DequeueForConsumer(qqueue_t *pThis, wti_t *pWti, int *const pSkippedMsgs) {
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    ISOBJ_TYPE_assert(pWti, wti);

    /* Do not overwrite a prior source before its completion at the front of
     * DequeueConsumableElements(). */
    CHKiRet(qqueueBindWtiSource(pThis, pWti));

retry_dequeue:
    CHKiRet(DequeueConsumable(pThis, pWti, pSkippedMsgs));

    if (pWti->batch.nElem == 0) ABORT_FINALIZE(RS_RET_IDLE);

finalize_it:
    /* An idle acquisition has no batch to retire. All other non-OK paths keep
     * their attribution so a store context or partial dequeue cannot be
     * completed through a callback owner's queue by mistake. */
    if (iRet == RS_RET_IDLE &&
        !qqueueLeaseHasResponsibility(pWti->batch.nElem, pWti->batch.nElemDeq, pWti->batch.storeData)) {
        const rsRetVal clearRet = qqueueClearWtiSource(pThis, pWti);
        if (clearRet != RS_RET_OK) iRet = clearRet;
    }
    if (iRet != RS_RET_OK && pWti->n_deferred_msgs != 0) {
        qqueueDrainDeferredLocked(pThis, pWti);
        /* An enqueue during disposal could not signal us as a waiter yet.
         * Retest under the mutex before returning IDLE to the wait loop. */
        if (iRet == RS_RET_IDLE && getLogicalQueueSize(pThis) > 0) goto retry_dequeue;
    }
    RETiRet;
}


/* This is called when a batch is processed and the worker does not
 * ask for another batch (e.g. because it is to be terminated)
 * Note that we must not be terminated while we delete a processed
 * batch. Otherwise, we may not complete it, and then the cancel
 * handler also tries to delete the batch. But then it finds some of
 * the messages already destructed. This was a bug we have seen, especially
 * with disk mode, where a delete takes rather long. Anyhow, the coneptual
 * problem exists in all queue modes.
 * rgerhards, 2009-05-27
 */
static rsRetVal batchProcessed(qqueue_t *pThis, wti_t *pWti) {
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    ISOBJ_TYPE_assert(pWti, wti);

    int iCancelStateSave;
    qqueue_t *const pSource = pWti->source_queue;
    /* DeleteProcessedBatch() defers final message destruction by resetting
     * the batch counters, so retain the dequeue count for checkpointing. */
    const int nElemDeq = pWti->batch.nElemDeq;
    /* at this spot, we must not be cancelled */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
    if (pSource != NULL) {
        /* The callback owner can differ from the physical source. The source
         * is authoritative after the pool-user and mutex validation above. */
        CHKiRet(qqueueBindWtiSource(pSource, pWti));
    }
    iRet = DeleteProcessedBatch(pSource == NULL ? pThis : pSource, pWti);
    if (iRet == RS_RET_OK) qqueueChkPersist(pSource == NULL ? pThis : pSource, nElemDeq);
    qqueueDrainDeferredLocked(pSource == NULL ? pThis : pSource, pWti);

finalize_it:
    pthread_setcancelstate(iCancelStateSave, NULL);

    RETiRet;
}

#ifdef ENABLE_TESTBENCH
static rsRetVal ConsumerReg(qqueue_t *pThis, wti_t *pWti);

/* This fixture reaches the production terminal-completion functions with a
 * source queue and a separate callback owner. It uses a completion callback
 * as a queue-layer fault seam: no store writes occur, but the source lease,
 * opaque storeData retention, counter, and deferred-cleanup branches execute.
 * It intentionally does not construct segmented-store on-disk state. */
static rsRetVal qqueueLeaseTestCompleteOK(qqueue_t *pThis,
                                          batch_t __attribute__((unused)) * pBatch,
                                          int *const committed,
                                          int *const retried) {
    ++pThis->segdiskCorruptionEvents;
    *committed = 1;
    *retried = 0;
    return RS_RET_OK;
}

static rsRetVal qqueueLeaseTestCompleteFail(qqueue_t *pThis,
                                            batch_t __attribute__((unused)) * pBatch,
                                            int *const committed,
                                            int *const retried) {
    ++pThis->segdiskCorruptionEvents;
    *committed = 0;
    *retried = 0;
    return RS_RET_IO_ERROR;
}

static void qqueueLeaseTestInitQueue(qqueue_t *const pThis, pthread_mutex_t *const mut, objInfo_t *const info) {
    memset(pThis, 0, sizeof(*pThis));
    pThis->objData.pObjInfo = info;
    #ifndef NDEBUG
    pThis->objData.iObjCooCKiE = 0xBADEFEE;
    #endif
    pThis->qType = QUEUETYPE_SEGMENTED_DISK;
    pThis->mut = mut;
    pthread_cond_init(&pThis->notFull, NULL);
    pThis->iQueueSize = 1;
    pThis->nLogDeq = 1;
    INIT_ATOMIC_HELPER_MUT(pThis->mutQueueSize);
    INIT_ATOMIC_HELPER_MUT(pThis->mutLogDeq);
}

/* Exercise the real regular consumer against an empty, initialized local BE
 * with a newly constructed worker. ConsumerReg() is normally entered from a
 * worker thread with cancellation disabled, so preserve that caller contract
 * while the imdiag command invokes it synchronously. The worker's shutdown
 * pointer must remain NULL: an empty acquisition has no batch and must not
 * need action-facing shutdown state. Both queue-flag values cover the idle
 * finalizer without letting another BE worker observe the temporary flag,
 * because this function holds the BE mutex until it restores zero. */
static rsRetVal qqueueLeaseTestEmptyLocalBackend(qqueue_t *const owner) {
    wti_t *worker = NULL;
    int cancelState;
    int locked = 0;
    DEFiRet;

    if (owner == NULL || owner->local == NULL || owner->pWtpReg == NULL || owner->pWtpReg->pUsr != owner ||
        owner->pWtpReg->pmutUsr != owner->mut)
        return RS_RET_PARAM_ERROR;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
    CHKiRet(wtiConstruct(&worker));
    CHKiRet(wtiSetpWtp(worker, owner->pWtpReg));
    CHKiRet(wtiConstructFinalize(worker));
    if (worker->pbShutdownImmediate != NULL || worker->source_queue != NULL || worker->logical_owner != NULL ||
        qqueueLeaseHasResponsibility(worker->batch.nElem, worker->batch.nElemDeq, worker->batch.storeData) ||
        worker->n_deferred_msgs != 0) {
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    d_pthread_mutex_lock(owner->mut);
    locked = 1;
    if (getLogicalQueueSize(owner) != 0 || owner->nLogDeq != 0 || qqueueIsShutdownImmediate(owner)) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    for (int immediate = 0; immediate <= 1; ++immediate) {
        qqueueSetShutdownImmediate(owner, immediate);
        iRet = ConsumerReg(owner, worker);
        qqueueSetShutdownImmediate(owner, 0);
        if (iRet != RS_RET_IDLE || worker->pbShutdownImmediate != NULL || worker->source_queue != NULL ||
            worker->logical_owner != NULL ||
            qqueueLeaseHasResponsibility(worker->batch.nElem, worker->batch.nElemDeq, worker->batch.storeData) ||
            worker->n_deferred_msgs != 0 || getLogicalQueueSize(owner) != 0 || owner->nLogDeq != 0) {
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }
    }
    iRet = RS_RET_OK;

finalize_it:
    if (locked) d_pthread_mutex_unlock(owner->mut);
    if (worker != NULL) wtiDestruct(&worker);
    pthread_setcancelstate(cancelState, NULL);
    RETiRet;
}

/* Exercise the real terminal completion path with a source queue distinct
 * from the callback owner, then calls the empty local-BE ConsumerReg fixture
 * against the initialized daemon queue. */
rsRetVal qqueueTestLeaseCompletionPaths(qqueue_t *const owner) {
    qqueue_t source;
    qqueue_t callback_owner;
    wtp_t pool;
    wti_t worker;
    pthread_mutex_t source_mut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t callback_mut = PTHREAD_MUTEX_INITIALIZER;
    objInfo_t queue_info = {.pszID = UCHAR_CONSTANT("qqueue")};
    objInfo_t wti_info = {.pszID = UCHAR_CONSTANT("wti")};
    int store_context;
    smsg_t retained_message;
    batch_obj_t retained_element = {.pMsg = &retained_message};
    batch_state_t retained_state = BATCH_STATE_COMM;
    DEFiRet;

    qqueueLeaseTestInitQueue(&source, &source_mut, &queue_info);
    qqueueLeaseTestInitQueue(&callback_owner, &callback_mut, &queue_info);
    /* Model a DA child whose logical root receives the completion callback. */
    source.pqParent = &callback_owner;
    callback_owner.iQueueSize = 17;
    callback_owner.nLogDeq = 17;
    memset(&pool, 0, sizeof(pool));
    pool.pUsr = &source;
    pool.pmutUsr = &source_mut;
    memset(&worker, 0, sizeof(worker));
    worker.objData.pObjInfo = &wti_info;
    #ifndef NDEBUG
    worker.objData.iObjCooCKiE = 0xBADEFEE;
    #endif
    worker.pWtp = &pool;
    worker.source_queue = &source;
    worker.logical_owner = &callback_owner;
    worker.batch.maxElem = 1;
    worker.batch.nElemDeq = 1;
    worker.batch.storeData = &store_context;
    CHKmalloc(worker.p_deferred_msgs = calloc(1, sizeof(smsg_t *)));

    source.qCompleteBatch = qqueueLeaseTestCompleteOK;
    d_pthread_mutex_lock(&source_mut);
    iRet = batchProcessed(&callback_owner, &worker);
    d_pthread_mutex_unlock(&source_mut);
    /* The synthetic source is not registered with the daemon. Compensate the
     * diagnostic aggregate that normal physical completion updates so this
     * isolated fixture cannot perturb the initialized main queue's oracle. */
    if (iRet == RS_RET_OK) qqueueAddOverallQueueSize(1);
    if (iRet != RS_RET_OK || source.segdiskCorruptionEvents != 1 || source.iQueueSize != 0 || source.nLogDeq != 0 ||
        callback_owner.segdiskCorruptionEvents != 0 || callback_owner.iQueueSize != 17 ||
        callback_owner.nLogDeq != 17 || worker.source_queue != NULL || worker.logical_owner != NULL ||
        worker.batch.storeData != NULL || worker.batch.nElemDeq != 0 || worker.n_deferred_msgs != 0) {
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    source.iQueueSize = 1;
    source.nLogDeq = 1;
    source.segdiskCorruptionEvents = 0;
    worker.source_queue = &source;
    worker.logical_owner = &callback_owner;
    worker.batch.nElem = 1;
    worker.batch.nElemDeq = 1;
    worker.batch.pElem = &retained_element;
    worker.batch.eltState = &retained_state;
    worker.batch.storeData = &store_context;
    source.qCompleteBatch = qqueueLeaseTestCompleteFail;
    d_pthread_mutex_lock(&source_mut);
    iRet = batchProcessed(&callback_owner, &worker);
    d_pthread_mutex_unlock(&source_mut);
    if (iRet != RS_RET_IO_ERROR || source.segdiskCorruptionEvents != 1 || source.iQueueSize != 1 ||
        source.nLogDeq != 1 || callback_owner.segdiskCorruptionEvents != 0 || worker.source_queue != &source ||
        worker.logical_owner != &callback_owner || worker.batch.storeData != &store_context ||
        worker.batch.nElem != 1 || worker.batch.nElemDeq != 1 || worker.batch.pElem != &retained_element ||
        worker.batch.eltState != &retained_state || retained_element.pMsg != &retained_message ||
        retained_state != BATCH_STATE_COMM || worker.n_deferred_msgs != 0) {
        iRet = RS_RET_INTERNAL_ERROR;
        goto finalize_it;
    }

    /* Restore a consuming completion callback and retire the retained opaque
     * context through the same terminal path before dismantling the fixture. */
    source.qCompleteBatch = qqueueLeaseTestCompleteOK;
    worker.batch.nElem = 0;
    worker.batch.pElem = NULL;
    worker.batch.eltState = NULL;
    d_pthread_mutex_lock(&source_mut);
    iRet = batchProcessed(&callback_owner, &worker);
    d_pthread_mutex_unlock(&source_mut);
    if (iRet == RS_RET_OK) qqueueAddOverallQueueSize(1);
    if (iRet != RS_RET_OK || source.segdiskCorruptionEvents != 2 || source.iQueueSize != 0 || source.nLogDeq != 0 ||
        worker.source_queue != NULL || worker.logical_owner != NULL || worker.batch.storeData != NULL ||
        worker.batch.nElemDeq != 0 || worker.n_deferred_msgs != 0) {
        iRet = RS_RET_INTERNAL_ERROR;
    }

    if (iRet == RS_RET_OK) iRet = qqueueLeaseTestEmptyLocalBackend(owner);

finalize_it:
    free(worker.p_deferred_msgs);
    DESTROY_ATOMIC_HELPER_MUT(source.mutQueueSize);
    DESTROY_ATOMIC_HELPER_MUT(source.mutLogDeq);
    DESTROY_ATOMIC_HELPER_MUT(callback_owner.mutQueueSize);
    DESTROY_ATOMIC_HELPER_MUT(callback_owner.mutLogDeq);
    pthread_cond_destroy(&source.notFull);
    pthread_cond_destroy(&callback_owner.notFull);
    pthread_mutex_destroy(&source_mut);
    pthread_mutex_destroy(&callback_mut);
    RETiRet;
}
#endif


/* This is the queue consumer in the regular (non-DA) case. It is
 * protected by the queue mutex, but MUST release it as soon as possible.
 * rgerhards, 2008-01-21
 */
static rsRetVal ConsumerReg(qqueue_t *pThis, wti_t *pWti) {
    int iCancelStateSave = PTHREAD_CANCEL_DISABLE;
    int bNeedReLock = 0; /**< do we need to lock the mutex again? */
    int skippedMsgs = 0; /**< did the queue loose any messages (can happen with
                          ** disk queue if .qi file is corrupt */
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    ISOBJ_TYPE_assert(pWti, wti);

    iRet = DequeueForConsumer(pThis, pWti, &skippedMsgs);
    if (iRet == RS_RET_FILE_NOT_FOUND) {
        /* This is a fatal condition and means the queue is almost unusable */
        d_pthread_mutex_unlock(pThis->mut);
        DBGOPRINT((obj_t *)pThis, "got 'file not found' error %d, queue defunct\n", iRet);
        iRet = queueSwitchToEmergencyMode(pThis, iRet);
        // TODO: think about what to return as iRet -- keep RS_RET_FILE_NOT_FOUND?
        qqueueLock(pThis);
    }
    if (iRet != RS_RET_OK) {
        FINALIZE;
    }

    /* we now have a non-idle batch of work, so we can release the queue mutex and process it */
    d_pthread_mutex_unlock(pThis->mut);
    bNeedReLock = 1;
    qqueueDrainDeferred(pWti);

    /* report errors, now that we are outside of queue lock */
    if (skippedMsgs > 0) {
        if (pThis->qType != QUEUETYPE_SEGMENTED_DISK)
            LogError(0, 0,
                     "problem on disk queue '%s': "
                     "queue files contain %d messages fewer than specified "
                     "in .qi file -- we lost those messages. That's all we know.",
                     obj.GetName((obj_t *)pThis), skippedMsgs);
    }

    /* at this spot, we may be cancelled */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &iCancelStateSave);


    qqueueSetWtiShutdownImmediate(pWti->source_queue == NULL ? pThis : pWti->source_queue, pWti);
    CHKiRet(pThis->pConsumer(pThis->pAction, &pWti->batch, pWti));

    /* we now need to check if we should deliberately delay processing a bit
     * and, if so, do that. -- rgerhards, 2008-01-30
     */
    if (pThis->iDeqSlowdown) {
        DBGOPRINT((obj_t *)pThis, "sleeping %d microseconds as requested by config params\n", pThis->iDeqSlowdown);
        srSleep(pThis->iDeqSlowdown / 1000000, pThis->iDeqSlowdown % 1000000);
    }

finalize_it:
    /* Consumer errors also leave cancellation disabled before taking the
     * queue mutex. The cancel handler therefore always enters unlocked. */
    pthread_setcancelstate(iCancelStateSave, NULL);
    DBGPRINTF("regular consumer finished, iret=%d, szlog %d sz phys %d\n", iRet, getLogicalQueueSize(pThis),
              getPhysicalQueueSize(pThis));

    /* now we are done, but potentially need to re-acquire the mutex */
    if (bNeedReLock) qqueueLock(pThis);
    /* The local BE is this pool's physical source (enforced by
     * qqueueBindWtiSource). A first empty dequeue has not initialized the
     * action-facing worker shutdown pointer, so read the queue flag directly.
     * Keep this check on error exits too: failed callbacks can leave COMM
     * entries whose delivery is ambiguous during immediate shutdown. */
    if (qqueueLocalWorker(pWti) && qqueueIsShutdownImmediate(pThis)) qqueueLocalRetainAmbiguous(pWti);

    RETiRet;
}


/* This is a special consumer to feed the disk-queue in disk-assisted mode.
 * When active, our own queue more or less acts as a memory buffer to the disk.
 * So this consumer just needs to drain the memory queue and submit entries
 * to the disk queue. The disk queue will then call the actual consumer from
 * the app point of view (we chain two queues here).
 * When this method is entered, the mutex is always locked and needs to be unlocked
 * as part of the processing.
 * rgerhards, 2008-01-14
 */
static rsRetVal ConsumerDA(qqueue_t *pThis, wti_t *pWti) {
    int i;
    int iCancelStateSave = PTHREAD_CANCEL_DISABLE;
    int bNeedReLock = 0; /**< do we need to lock the mutex again? */
    int skippedMsgs = 0;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    ISOBJ_TYPE_assert(pWti, wti);

    /* Graph DA keeps its one activated transfer thread parked between spills.
     * Retiring the only slot at low water leaves a refill window where advice
     * sees an exiting, not-yet-joinable WTI and cannot restart it. No producer
     * need arrive after that slot finally becomes free. Retaining the idle
     * thread closes that window without changing high/low-water hysteresis.
     *
     * Complete the prior lease before parking. Completion may drop the queue
     * mutex for deferred destruction, so recheck the activation predicate on
     * return. WTI registers its idle wait under this same mutex; high-water
     * advice therefore either wins this recheck or signals the registered wait.
     * Save/shutdown use the pool state to bypass runtime parking entirely. */
    if (pThis->localGraphConf != NULL &&
        ATOMIC_LOAD_32BIT((int *)&pThis->pWtpDA->wtpState, &pThis->pWtpDA->mutWtpState) == wtpState_RUNNING &&
        !pThis->localDAActive) {
        CHKiRet(batchProcessed(pThis, pWti));
        if (!pThis->localDAActive) ABORT_FINALIZE(RS_RET_IDLE);
    }

    CHKiRet(DequeueForConsumer(pThis, pWti, &skippedMsgs));
    qqueue_t *const diskDestination = pThis->pqDA;
    if (diskDestination == NULL) ABORT_FINALIZE(RS_RET_IDLE);

    /* we now have a non-idle batch of work, so we can release the queue mutex and process it */
    d_pthread_mutex_unlock(pThis->mut);
    bNeedReLock = 1;
    qqueueDrainDeferred(pWti);

    /* at this spot, we may be cancelled */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &iCancelStateSave);

    /* qqueueEnqMsg disables cancellation internally. For graph DA workers,
     * extend that existing protected region through the acceptance-state store
     * so cancellation cannot replay a successfully persisted source reference. */
    if (pThis->localGraphConf != NULL) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    /* iterate over returned results and enqueue them in DA queue */
    for (i = 0; i < pWti->batch.nElem && !qqueueIsShutdownImmediate(pThis); i++) {
        iRet = qqueueEnqMsg(diskDestination, eFLOWCTL_NO_DELAY, MsgAddRef(pWti->batch.pElem[i].pMsg));
        if (iRet != RS_RET_OK) {
            if (iRet == RS_RET_ERR_QUEUE_EMERGENCY) {
                /* Queue emergency error occurred */
                DBGOPRINT((obj_t *)pThis,
                          "ConsumerDA:qqueueEnqMsg caught RS_RET_ERR_QUEUE_EMERGENCY,"
                          "aborting loop.\n");
                FINALIZE;
            } else {
                DBGOPRINT((obj_t *)pThis,
                          "ConsumerDA:qqueueEnqMsg item (%d) returned "
                          "with error state: '%d'\n",
                          i, iRet);
                if (diskDestination->qType == QUEUETYPE_SEGMENTED_DISK) {
                    /* The segmented child may fail before the event becomes
                     * durable (for example while publishing its engine marker
                     * or materializing the store). Leave this and the
                     * remaining batch elements uncommitted so normal parent
                     * retry handling preserves them. Classic DA behavior is
                     * intentionally unchanged. */
                    break;
                }
            }
        }
        pWti->batch.eltState[i] = iRet != RS_RET_OK && pThis->local != NULL ? BATCH_STATE_DISC : BATCH_STATE_COMM;
        if (iRet == RS_RET_OK && pThis->local != NULL) {
            qqueueLock(pThis);
            qqueueLocalDiskTransferred(pThis, 1);
            d_pthread_mutex_unlock(pThis->mut);
        }
    }

finalize_it:
    /* Early errors must also restore the queue-locked cancellation contract. */
    pthread_setcancelstate(iCancelStateSave, NULL);
    /*	Check the last return state of qqueueEnqMsg. If an error was returned, we acknowledge it only.
     *	Unless the error code is RS_RET_ERR_QUEUE_EMERGENCY, we reset the return state to RS_RET_OK.
     *	Otherwise the Caller functions would run into an infinite Loop trying to enqueue the
     *	same messages over and over again.
     *
     *	However we do NOT overwrite positive return states like
     *		RS_RET_TERMINATE_NOW,
     *		RS_RET_NO_RUN,
     *		RS_RET_IDLE,
     *		RS_RET_TERMINATE_WHEN_IDLE
     *	These return states are important for Queue handling of the upper laying functions.
     *	RGer: Note that checking for iRet < 0 is a bit bold. In theory, positive iRet
     *	values are "OK" states, and things that the caller shall deal with. However,
     *	this has not been done so consistently. Andre convinced me that the current
     *	code is an elegant solution. However, if problems with queue workers and/or
     *	shutdown come up, this code here should be looked at suspiciously. In those
     *	cases it may work out to check all status codes explicitely, just to avoid
     *	a pitfall due to unexpected states being passed on to the caller.
     */
    if (iRet != RS_RET_OK && iRet != RS_RET_ERR_QUEUE_EMERGENCY && iRet < 0) {
        DBGOPRINT((obj_t *)pThis, "ConsumerDA:qqueueEnqMsg Resetting iRet from %d back to RS_RET_OK\n", iRet);
        iRet = RS_RET_OK;
    } else {
        DBGOPRINT((obj_t *)pThis, "ConsumerDA:qqueueEnqMsg returns with iRet %d\n", iRet);
    }

    /* now we are done, but potentially need to re-acquire the mutex */
    if (bNeedReLock) qqueueLock(pThis);

    RETiRet;
}


/* must only be called when the queue mutex is locked, else results
 * are not stable!
 */
static rsRetVal qqueueChkStopWrkrDA(qqueue_t *pThis) {
    DEFiRet;

    DBGPRINTF("rger: chkStopWrkrDA called, low watermark %d, log Size %d, phys Size %d, bEnqOnly %d\n",
              pThis->iLowWtrMrk, getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis), pThis->bEnqOnly);
    if (pThis->bEnqOnly) {
        iRet = RS_RET_TERMINATE_WHEN_IDLE;
    }
    if (getPhysicalQueueSize(pThis) <= pThis->iLowWtrMrk) {
        pThis->localDAActive = 0;
        qqueueLocalWakeBackendHelpers(pThis);
        if (pThis->localGraphConf == NULL) iRet = RS_RET_TERMINATE_NOW;
    }

    RETiRet;
}


/* must only be called when the queue mutex is locked, else results
 * are not stable!
 * Version for the regular worker thread. DA child queues intentionally keep
 * their regular worker alive while idle so worker-instance state, such as
 * output connections, survives short refill gaps.
 */
static rsRetVal ChkStopWrkrReg(qqueue_t *pThis) {
    DEFiRet;
    /*DBGPRINTF("XXXX: chkStopWrkrReg called, low watermark %d, log Size %d, phys Size %d, bEnqOnly %d\n",
    pThis->iLowWtrMrk, getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis), pThis->bEnqOnly);*/
    if (pThis->bEnqOnly) {
        iRet = RS_RET_TERMINATE_NOW;
    }

    RETiRet;
}


/* return the configured "deq max at once" interval
 * rgerhards, 2009-04-22
 */
static rsRetVal GetDeqBatchSize(qqueue_t *pThis, int *pVal) {
    DEFiRet;
    assert(pVal != NULL);
    *pVal = pThis->iDeqBatchSize;
    RETiRet;
}


static void qqueueLocalLegacyRead(statsobj_t *const stats, void *const owner) {
    (void)stats;
    qqueueLocalRefreshLegacy(owner);
}


/* start up the queue - it must have been constructed and parameters defined
 * before.
 */
rsRetVal qqueueStart(rsconf_t *cnf, qqueue_t *pThis) /* this is the ConstructionFinalizer */
{
    DEFiRet;
    uchar pszBuf[64];
    uchar pszQIFNam[MAXFNAME];
    int wrk;
    uchar *qName;
    size_t lenBuf;

    assert(pThis != NULL);

    if (pThis->bLocalScope) {
        if (!pThis->bLocalConfigValidated || pThis->bLocalConfigError) ABORT_FINALIZE(RS_RET_LOCAL_QUEUE_CONFIG);
        /* The regular FixedArray pool remains the physical BE. After its
         * construction, local startup preallocates the FE family and enables
         * the local-only admission wrapper. Any failure is fatal to startup. */
    }

    /* do not modify the queue if it's already running(happens when dynamic config reload is invoked
     * and the queue is used in the new config as well)
     */
    if (pThis->isRunning) FINALIZE;

    dbgoprint((obj_t *)pThis, "starting queue\n");

    if (pThis->pszSpoolDir == NULL) {
        /* note: we need to pick the path so late as we do not have
         *       the workdir during early config load
         */
        if ((pThis->pszSpoolDir = (uchar *)strdup((char *)glbl.GetWorkDir(cnf))) == NULL)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        pThis->lenSpoolDir = ustrlen(pThis->pszSpoolDir);
    }
    if (pThis->qType == QUEUETYPE_SEGMENTED_DISK && pThis->pszSpoolDir[0] == '\0') {
        LogError(0, RS_RET_CONF_PARAM_INVLD,
                 "error on queue '%s': segmentedDisk requires queue.spoolDirectory or a global workDirectory",
                 obj.GetName((obj_t *)pThis));
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }
    /* set type-specific handlers and other very type-specific things
     * (we can not totally hide it...)
     */
    pThis->qDeqBatch = NULL;
    pThis->qCompleteBatch = NULL;
    switch (pThis->qType) {
        case QUEUETYPE_FIXED_ARRAY:
            pThis->qConstruct = qConstructFixedArray;
            pThis->qDestruct = qDestructFixedArray;
            pThis->qAdd = qAddFixedArray;
            pThis->qDeq = qDeqFixedArray;
            pThis->qDel = qDelFixedArray;
            pThis->MultiEnq = qqueueMultiEnqObjNonDirect;
            break;
        case QUEUETYPE_LINKEDLIST:
            pThis->qConstruct = qConstructLinkedList;
            pThis->qDestruct = qDestructLinkedList;
            pThis->qAdd = qAddLinkedList;
            pThis->qDeq = qDeqLinkedList;
            pThis->qDel = qDelLinkedList;
            pThis->MultiEnq = qqueueMultiEnqObjNonDirect;
            break;
        case QUEUETYPE_DISK:
            pThis->qConstruct = qConstructDisk;
            pThis->qDestruct = qDestructDisk;
            pThis->qAdd = qAddDisk;
            pThis->qDeq = qDeqDisk;
            pThis->qDel = NULL; /* delete for disk handled via special code! */
            pThis->MultiEnq = qqueueMultiEnqObjNonDirect;
            /* pre-construct file name for .qi file */
            pThis->lenQIFNam = snprintf((char *)pszQIFNam, sizeof(pszQIFNam), "%s/%s.qi", (char *)pThis->pszSpoolDir,
                                        (char *)pThis->pszFilePrefix);
            pThis->pszQIFNam = ustrdup(pszQIFNam);
            DBGOPRINT((obj_t *)pThis, ".qi file name is '%s', len %d\n", pThis->pszQIFNam, (int)pThis->lenQIFNam);
            break;
        case QUEUETYPE_SEGMENTED_DISK:
            pThis->qConstruct = qConstructSegDisk;
            pThis->qDestruct = qDestructSegDisk;
            pThis->qAdd = qAddSegDisk;
            pThis->qDeq = NULL;
            pThis->qDel = NULL;
            pThis->qDeqBatch = qDeqBatchSegDisk;
            pThis->qCompleteBatch = qCompleteBatchSegDisk;
            pThis->MultiEnq = qqueueMultiEnqObjNonDirect;
            break;
        case QUEUETYPE_DIRECT:
            pThis->qConstruct = qConstructDirect;
            pThis->qDestruct = qDestructDirect;
            /* these entry points shall not be used in direct mode
             * To catch program errors, make us abort if that happens!
             * rgerhards, 2013-11-05
             */
            pThis->qAdd = qAddDirect;
            pThis->MultiEnq = qqueueMultiEnqObjDirect;
            pThis->qDel = NULL;
            break;
        default:
            // We need to satisfy compiler which does not properly handle enum
            break;
    }

    /* finalize some initializations that could not yet be done because it is
     * influenced by properties which might have been set after queueConstruct ()
     */
    if (pThis->pqParent == NULL) {
        CHKmalloc(pThis->mut = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t)));
        pthread_mutex_init(pThis->mut, NULL);
    } else {
        /* child queue, we need to use parent's mutex */
        DBGOPRINT((obj_t *)pThis, "I am a child\n");
        pThis->mut = pThis->pqParent->mut;
    }

    pthread_mutex_init(&pThis->mutThrdMgmt, NULL);
    pthread_cond_init(&pThis->notFull, NULL);
    pthread_cond_init(&pThis->belowFullDlyWtrMrk, NULL);
    pthread_cond_init(&pThis->belowLightDlyWtrMrk, NULL);

    /* call type-specific constructor */
    CHKiRet(pThis->qConstruct(pThis)); /* this also sets bIsDA */

    /* re-adjust some params if required */
    if (pThis->bIsDA) {
        /* if we are in DA mode, we must make sure full delayable messages do not
         * initiate going to disk!
         */
        wrk = pThis->iHighWtrMrk - (pThis->iHighWtrMrk / 100) * 50; /* 50% of high water mark */
        if (wrk < pThis->iFullDlyMrk) pThis->iFullDlyMrk = wrk;
    }

    DBGOPRINT((obj_t *)pThis,
              "params: type %d, enq-only %d, disk assisted %d, spoolDir '%s', maxFileSz %lld, "
              "maxQSize %d, lqsize %d, pqsize %d, child %d, full delay %d, "
              "light delay %d, deq batch size %d, min deq batch size %d, "
              "high wtrmrk %d, low wtrmrk %d, "
              "discardmrk %d, max wrkr %d, min msgs f. wrkr %d "
              "takeFlowCtlFromMsg %d\n",
              pThis->qType, pThis->bEnqOnly, pThis->bIsDA, pThis->pszSpoolDir, pThis->iMaxFileSize,
              pThis->iMaxQueueSize, getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis),
              pThis->pqParent == NULL ? 0 : 1, pThis->iFullDlyMrk, pThis->iLightDlyMrk, pThis->iDeqBatchSize,
              pThis->iMinDeqBatchSize, pThis->iHighWtrMrk, pThis->iLowWtrMrk, pThis->iDiscardMrk,
              (int)pThis->iNumWorkerThreads, (int)pThis->iMinMsgsPerWrkr, pThis->takeFlowCtlFromMsg);

    pThis->bQueueStarted = 1;
    if (pThis->qType == QUEUETYPE_DIRECT) FINALIZE; /* with direct queues, we are already finished... */

    /* create worker thread pools for regular and DA operation.
     */
    lenBuf = snprintf((char *)pszBuf, sizeof(pszBuf), "%.*s:Reg", (int)(sizeof(pszBuf) - 16),
                      obj.GetName((obj_t *)pThis)); /* leave some room inside the name for suffixes */
    if (lenBuf >= sizeof(pszBuf)) {
        LogError(0, RS_RET_INTERNAL_ERROR,
                 "%s:%d debug header too long: %zd - in "
                 "thory this cannot happen - truncating",
                 __FILE__, __LINE__, lenBuf);
        lenBuf = sizeof(pszBuf) - 1;
        pszBuf[lenBuf] = '\0';
    }
    CHKiRet(wtpConstruct(&pThis->pWtpReg));
    CHKiRet(wtpSetDbgHdr(pThis->pWtpReg, pszBuf, lenBuf));
    CHKiRet(wtpSetpfRateLimiter(pThis->pWtpReg, (rsRetVal(*)(void *pUsr))RateLimiter));
    CHKiRet(wtpSetpfChkStopWrkr(pThis->pWtpReg, (rsRetVal(*)(void *pUsr, int))ChkStopWrkrReg));
    CHKiRet(wtpSetpfGetDeqBatchSize(pThis->pWtpReg, (rsRetVal(*)(void *pUsr, int *))GetDeqBatchSize));
    CHKiRet(wtpSetpfDoWork(pThis->pWtpReg, (rsRetVal(*)(void *pUsr, void *pWti))ConsumerReg));
    CHKiRet(wtpSetpfObjProcessed(pThis->pWtpReg, (rsRetVal(*)(void *pUsr, wti_t *pWti))batchProcessed));
    CHKiRet(wtpSetpmutUsr(pThis->pWtpReg, pThis->mut));
    CHKiRet(wtpSetiNumWorkerThreads(pThis->pWtpReg, pThis->iNumWorkerThreads));
    CHKiRet(wtpSettoWrkShutdown(pThis->pWtpReg, pThis->toWrkShutdown));
    /* This callback implements runtime idle dematerialization only.  Process
     * shutdown deliberately stops and joins the workers without waiting for
     * this timer; qDestructSegDisk() then closes the store and, when empty,
     * removes its segments, state file, and directory synchronously. */
    if (pThis->segdiskDAChild && pThis->diskQueueIdleTimeout != -1) {
        CHKiRet(wtpSetbAllowFirstWorkerToTimeout(pThis->pWtpReg, 1));
        CHKiRet(wtpSetpfIdleTimeout(pThis->pWtpReg, (rsRetVal(*)(void *pUsr))qqueueSegDiskIdleTimeout));
        CHKiRet(wtpSettoFirstWrkShutdown(pThis->pWtpReg, pThis->diskQueueIdleTimeout));
    } else if (pThis->segdiskDAChild) {
        /* An idle timeout of -1 disables dematerialization without disabling
         * the normal shutdown timeout for additional workers.  Explicitly
         * keep slot zero non-timeout-capable: wtpStartWrkr() consequently
         * marks that slot always-running, so the generic worker timeout can
         * never terminate the segmented DA child's final worker. */
        CHKiRet(wtpSetbAllowFirstWorkerToTimeout(pThis->pWtpReg, 0));
    }
    CHKiRet(wtpSetpUsr(pThis->pWtpReg, pThis));
    if (pThis->bLocalScope || pThis->localGraphConf != NULL) CHKiRet(wtpUseMonotonicTermination(pThis->pWtpReg));
    CHKiRet(wtpConstructFinalize(pThis->pWtpReg));

    /* Validate queue configuration before starting */
    if (pThis->qType == QUEUETYPE_DISK || pThis->qType == QUEUETYPE_SEGMENTED_DISK || pThis->bIsDA) {
        /* Check that maxDiskSpace is not smaller than maxFileSize */
        if (pThis->sizeOnDiskMax > 0 && pThis->iMaxFileSize > 0 && pThis->sizeOnDiskMax < pThis->iMaxFileSize) {
            LogError(0, RS_RET_CONF_PARAM_INVLD,
                     "queue.maxDiskSpace (%lld) must be larger than queue.maxFileSize (%lld) - "
                     "setting queue.maxDiskSpace to %lld",
                     pThis->sizeOnDiskMax, pThis->iMaxFileSize, pThis->iMaxFileSize);
            pThis->sizeOnDiskMax = pThis->iMaxFileSize;
        }
    }
    /* Recovery can start disk workers inside InitDA. Publish the local family
     * first so recovered obligations and downstream producers have an owner. */
    if (pThis->bLocalScope) CHKiRet(qqueueLocalStart(pThis));
    if (pThis->pqParent != NULL && pThis->pqParent->local != NULL)
        /* getPhysicalQueueSize may return a lazy-recovery wake sentinel of one.
         * Only discovered records are obligations in this run. */
        qqueueLocalDiskRestored(pThis->pqParent, (uint64_t)PREFER_FETCH_32BIT(pThis->iQueueSize));

    /* set up DA system if we have a disk-assisted queue */
    if (pThis->bIsDA) CHKiRet(InitDA(pThis, LOCK_MUTEX)); /* initiate DA mode */

    DBGOPRINT((obj_t *)pThis, "queue finished initialization\n");

    /* if the queue already contains data, we need to start the correct number of worker threads. This can be
     * the case when a disk queue has been loaded. If we did not start it here, it would never start.
     */
    /* qqueueAdviseMaxWorkers() publishes worker wakeup reservations under
     * the queue mutex. A top-level queue is started without that mutex held;
     * a DA child is started from InitDA(), which already holds its parent's
     * shared mutex. Keep the child path lock-free here to avoid relocking the
     * non-recursive parent mutex. */
    const int bNeedQueueLock = pThis->pqParent == NULL;
    if (bNeedQueueLock) qqueueLock(pThis);
    qqueueAdviseMaxWorkers(pThis);
    if (bNeedQueueLock) d_pthread_mutex_unlock(pThis->mut);

    /* support statistics gathering */
    qName = obj.GetName((obj_t *)pThis);
    CHKiRet(statsobj.Construct(&pThis->statsobj));
    CHKiRet(statsobj.SetName(pThis->statsobj, qName));
    CHKiRet(statsobj.SetOrigin(pThis->statsobj, (uchar *)"core.queue"));
    /* we need to save the queue size, as the stats module initializes it to 0! */
    /* iQueueSize is a dual-use counter: no init, no mutex! */
    CHKiRet(
        statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("size"), ctrType_Int, CTR_FLAG_NONE, &pThis->iQueueSize));

    STATSCOUNTER_INIT(pThis->ctrEnqueued, pThis->mutCtrEnqueued);
    CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("enqueued"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &pThis->ctrEnqueued));

    STATSCOUNTER_INIT(pThis->ctrSizeEnqueued, pThis->mutCtrSizeEnqueued);
    CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("size.enqueued"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &pThis->ctrSizeEnqueued));

    STATSCOUNTER_INIT(pThis->ctrFull, pThis->mutCtrFull);
    CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("full"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &pThis->ctrFull));

    STATSCOUNTER_INIT(pThis->ctrFDscrd, pThis->mutCtrFDscrd);
    CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("discarded.full"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &pThis->ctrFDscrd));
    STATSCOUNTER_INIT(pThis->ctrNFDscrd, pThis->mutCtrNFDscrd);
    CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("discarded.nf"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &pThis->ctrNFDscrd));

    if (pThis->bMutexContentionStats) {
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("mutex.contention"), ctrType_IntCtr,
                                    CTR_FLAG_RESETTABLE, &pThis->ctrMutexContention));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("mutex.wait_ns"), ctrType_IntCtr,
                                    CTR_FLAG_RESETTABLE, &pThis->ctrMutexWaitNs));
    }

    pThis->ctrMaxqsize = 0; /* no mutex needed, thus no init call */
    CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("maxqsize"), ctrType_Int, CTR_FLAG_NONE,
                                &pThis->ctrMaxqsize));

    if (pThis->qType == QUEUETYPE_SEGMENTED_DISK) {
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("disk.usage"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskBytes));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("segments"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskSegments));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("checkpoints"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskCheckpoints));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("replayed"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskReplayed));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("corruption.events"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskCorruptionEvents));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("corruption.bytes"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskCorruptionBytes));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("corruption.records"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskCorruptionRecords));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("corruption.segments"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskCorruptionSegments));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("retry.overage.bytes"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskRetryOverageBytes));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("retry.overage.maxbytes"), ctrType_Int,
                                    CTR_FLAG_NONE, &pThis->segdiskRetryOverageMaxBytes));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("state.writes"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskStateWrites));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("state.forcedWrites"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskForcedStateWrites));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("recovery.pending"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskRecoveryPending));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("recovery.bytes"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskRecoveryBytes));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("recovery.records"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->segdiskRecoveryRecords));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("startup.payloadBytesRead"), ctrType_Int,
                                    CTR_FLAG_NONE, &pThis->segdiskStartupPayloadBytes));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("startup.segmentFilesProbed"), ctrType_Int,
                                    CTR_FLAG_NONE, &pThis->segdiskStartupSegmentFilesProbed));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("store.materializations"), ctrType_Int,
                                    CTR_FLAG_NONE, &pThis->segdiskMaterializations));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("store.idleDematerializations"), ctrType_Int,
                                    CTR_FLAG_NONE, &pThis->segdiskDematerializations));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("store.idleCleanupFailures"), ctrType_Int,
                                    CTR_FLAG_NONE, &pThis->segdiskIdleCleanupFailures));
        CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("workers.current"), ctrType_Int, CTR_FLAG_NONE,
                                    &pThis->pWtpReg->iCurNumWrkThrd));
    }

    CHKiRet(statsobj.ConstructFinalize(pThis->statsobj));
    if (pThis->bLocalScope) {
        CHKiRet(statsobj.SetPreReadNotifier(pThis->statsobj, qqueueLocalLegacyRead, pThis));
        CHKiRet(qqueueLocalStatsConstruct(pThis, qName, (uint32_t)pThis->localMaxFrontends, pThis->localFrontendStats,
                                          &pThis->localStats));
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        /* note: a child uses it's parent mutex, so do not delete it! */
        if (pThis->pqParent == NULL && pThis->mut != NULL && !(pThis->bLocalScope && pThis->bQueueStarted))
            free(pThis->mut);
    } else {
        pThis->isRunning = 1;
    }
    RETiRet;
}


/* persist the queue to disk (write the .qi file). If we have something to persist, we first
 * save the information on the queue properties itself and then we call
 * the queue-type specific drivers.
 * Variable bIsCheckpoint is set to 1 if the persist is for a checkpoint,
 * and 0 otherwise.
 * rgerhards, 2008-01-10
 */
static rsRetVal qqueuePersist(qqueue_t *pThis, int bIsCheckpoint) {
    DEFiRet;
    char *tmpQIFName = NULL;
    strm_t *psQIF = NULL; /* Queue Info File */
    char errStr[1024];

    assert(pThis != NULL);

    if (pThis->qType == QUEUETYPE_SEGMENTED_DISK) {
        CHKiRet(segdiskStoreCheckpoint(pThis->tVars.segdisk, bIsCheckpoint != QUEUE_CHECKPOINT));
        qqueueUpdateSegDiskStats(pThis);
        FINALIZE;
    }

    if (pThis->qType != QUEUETYPE_DISK) {
        if (getPhysicalQueueSize(pThis) > 0) {
            /* This error code is OK, but we will probably not implement this any time
             * The reason is that persistence happens via DA queues. But I would like to
             * leave the code as is, as we so have a hook in case we need one.
             * -- rgerhards, 2008-01-28
             */
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        } else
            FINALIZE; /* if the queue is empty, we are happy and done... */
    }

    DBGOPRINT((obj_t *)pThis, "persisting queue to disk, %d entries...\n", getPhysicalQueueSize(pThis));

    if ((bIsCheckpoint != QUEUE_CHECKPOINT) && (getPhysicalQueueSize(pThis) == 0)) {
        if (pThis->bNeedDelQIF) {
            unlink((char *)pThis->pszQIFNam);
            pThis->bNeedDelQIF = 0;
        }
        /* indicate spool file needs to be deleted */
        if (pThis->tVars.disk.pReadDel != NULL) /* may be NULL if we had a startup failure! */
            CHKiRet(strm.SetbDeleteOnClose(pThis->tVars.disk.pReadDel, 1));
        FINALIZE; /* nothing left to do, so be happy */
    }

    int lentmpQIFName;
    lentmpQIFName = asprintf((char **)&tmpQIFName, "%s.tmp", pThis->pszQIFNam);
    if (tmpQIFName == NULL) tmpQIFName = (char *)pThis->pszQIFNam;

    CHKiRet(strm.Construct(&psQIF));
    CHKiRet(strm.SettOperationsMode(psQIF, STREAMMODE_WRITE_TRUNC));
    CHKiRet(strm.SetbSync(psQIF, pThis->bSyncQueueFiles));
    CHKiRet(strm.SetsType(psQIF, STREAMTYPE_FILE_SINGLE));
    CHKiRet(strm.SetbNoFollowFinal(psQIF, 1));
    CHKiRet(strm.SetFName(psQIF, (uchar *)tmpQIFName, lentmpQIFName));
    CHKiRet(strm.ConstructFinalize(psQIF));

    /* first, write the property bag for ourselfs
     * And, surprisingly enough, we currently need to persist only the size of the
     * queue. All the rest is re-created with then-current config parameters when the
     * queue is re-created. Well, we'll also save the current queue type, just so that
     * we know when somebody has changed the queue type... -- rgerhards, 2008-01-11
     */
    CHKiRet(obj.BeginSerializePropBag(psQIF, (obj_t *)pThis));
    objSerializeSCALAR(psQIF, iQueueSize, INT);
    objSerializeSCALAR(psQIF, tVars.disk.sizeOnDisk, INT64);
    CHKiRet(obj.EndSerialize(psQIF));

    /* now persist the stream info */
    if (pThis->tVars.disk.pWrite != NULL) CHKiRet(strm.Serialize(pThis->tVars.disk.pWrite, psQIF));
    if (pThis->tVars.disk.pReadDel != NULL) CHKiRet(strm.Serialize(pThis->tVars.disk.pReadDel, psQIF));

    strm.Destruct(&psQIF);
    if (tmpQIFName != (char *)pThis->pszQIFNam) { /* pointer, not string comparison! */
        if (rename(tmpQIFName, (char *)pThis->pszQIFNam) != 0) {
            rs_strerror_r(errno, errStr, sizeof(errStr));
            DBGOPRINT((obj_t *)pThis, "FATAL error: renaming temporary .qi file failed: %s\n", errStr);
            ABORT_FINALIZE(RS_RET_RENAME_TMP_QI_ERROR);
        }
    }

    /* tell the input file object that it must not delete the file on close if the queue
     * is non-empty - but only if we are not during a simple checkpoint
     */
    if (bIsCheckpoint != QUEUE_CHECKPOINT && pThis->tVars.disk.pReadDel != NULL) {
        CHKiRet(strm.SetbDeleteOnClose(pThis->tVars.disk.pReadDel, 0));
    }

    /* we have persisted the queue object. So whenever it comes to an empty queue,
     * we need to delete the QIF. Thus, we indicte that need.
     */
    pThis->bNeedDelQIF = 1;

finalize_it:
    if (tmpQIFName != (char *)pThis->pszQIFNam) /* pointer, not string comparison! */
        free(tmpQIFName);
    if (psQIF != NULL) strm.Destruct(&psQIF);

    RETiRet;
}
static rsRetVal qqueueChkPersist(qqueue_t *const pThis, const int nUpdates) {
    DEFiRet;
    ISOBJ_TYPE_assert(pThis, qqueue);
    assert(nUpdates >= 0);

    if (nUpdates == 0) FINALIZE;
    /* segmentedDisk checkpoints completed dequeue batches in its store. Enqueue
     * activity must not turn checkpointInterval=1 into one checkpoint per write. */
    if (pThis->qType == QUEUETYPE_SEGMENTED_DISK) FINALIZE;

    pThis->iUpdsSincePersist += nUpdates;
    if (pThis->iPersistUpdCnt && pThis->iUpdsSincePersist >= pThis->iPersistUpdCnt) {
        qqueuePersist(pThis, QUEUE_CHECKPOINT);
        pThis->iUpdsSincePersist = 0;
    }

finalize_it:
    RETiRet;
}


/* persist a queue with all data elements to disk - this is used to handle
 * bSaveOnShutdown. We utilize the DA worker to do this. This must only
 * be called after all workers have been shut down and if bSaveOnShutdown
 * is actually set. Note that this function may potentially run long,
 * depending on the queue configuration (e.g. store on remote machine).
 * rgerhards, 2009-05-26
 */
static rsRetVal DoSaveOnShutdown(qqueue_t *pThis) {
    struct timespec tTimeout;
    rsRetVal iRetLocal;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);

    /* we reduce the low water mark, otherwise the DA worker would terminate when
     * it is reached.
     */
    DBGOPRINT((obj_t *)pThis, "bSaveOnShutdown set, restarting DA worker...\n");
    qqueueSetShutdownImmediate(pThis, 0); /* would terminate the DA worker! */
    pThis->iLowWtrMrk = 0;
    wtpSetState(pThis->pWtpDA, wtpState_SHUTDOWN); /* shutdown worker (only) when done (was _IMMEDIATE!) */
    /* Unlike the normal enqueue advice path, destruction does not already
     * hold this pool's queue mutex. The targeted-wakeup predicates are
     * protected by that mutex, including a worker's exiting transition.
     */
    d_pthread_mutex_lock(pThis->pWtpDA->pmutUsr);
    wtpAdviseMaxWorkers(pThis->pWtpDA, 1, PERMIT_WORKER_START_DURING_SHUTDOWN); /* restart DA worker */
    d_pthread_mutex_unlock(pThis->pWtpDA->pmutUsr);

    DBGOPRINT((obj_t *)pThis, "waiting for DA worker to terminate...\n");
    timeoutComp(&tTimeout, QUEUE_TIMEOUT_ETERNAL);
    /* and run the primary queue's DA worker to drain the queue */
    iRetLocal = wtpShutdownAll(pThis->pWtpDA, wtpState_SHUTDOWN, &tTimeout);
    DBGOPRINT((obj_t *)pThis, "end queue persistence run, iRet %d, queue size log %d, phys %d\n", iRetLocal,
              getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis));
    if (iRetLocal != RS_RET_OK) {
        DBGOPRINT((obj_t *)pThis,
                  "unexpected iRet state %d after trying to shut down primary "
                  "queue in disk save mode, continuing, but results are unpredictable\n",
                  iRetLocal);
    }

    RETiRet;
}

/* Reuse the existing persistence worker, including its intentionally separate
 * (potentially long) save phase. Callback deadlines are never renewed here. */
rsRetVal qqueueSaveLocalBackend(qqueue_t *const owner) {
    if (!owner->bIsDA || !owner->bSaveOnShutdown || owner->pqDA == NULL) return RS_RET_OK;
    if (getPhysicalQueueSize(owner) == 0) return RS_RET_OK;
    const rsRetVal ret = DoSaveOnShutdown(owner);
    if (ret != RS_RET_OK) return ret;
    return getPhysicalQueueSize(owner) == 0 ? RS_RET_OK : RS_RET_QUEUE_FULL;
}

rsRetVal qqueueFinishLocalDisk(qqueue_t *const owner) {
    qqueue_t *const disk =
        owner->pqDA != NULL
            ? owner->pqDA
            : ((owner->qType == QUEUETYPE_DISK || owner->qType == QUEUETYPE_SEGMENTED_DISK) ? owner : NULL);
    if (disk == NULL) return RS_RET_OK;
    /* Workers are joined, so checkpoint and final inventory share exclusive
     * source ownership. The ordinary destructor still closes the same store. */
    const rsRetVal ret = qqueuePersist(disk, QUEUE_NO_CHECKPOINT);
    if (ret == RS_RET_OK)
        qqueueLocalDiskPersisted(owner, (uint64_t)PREFER_FETCH_32BIT(disk->iQueueSize));
    else
        LogError(0, ret, "%s: failed to finalize disk queue persistence", objGetName((obj_t *)owner));
    return ret;
}


/* destructor for the queue object */
BEGINobjDestruct(qqueue) /* be sure to specify the object type also in END and CODESTART macros! */
    CODESTARTobjDestruct(qqueue);
    /* Even a Direct or not-yet-started queue is a graph lifetime anchor.
     * Stop the graph before freeing its first node; later graph traversal must
     * never see a freed node after partial activation or validation cleanup. */
    if (pThis->localSource == NULL && pThis->pqParent == NULL && pThis->localGraphConf != NULL)
        rulesetShutdownLocalGraph(pThis->localGraphConf);
    if (pThis->localSource != NULL) {
        assert(pThis->pWtpReg == NULL);
        /* FE descriptor owns its mutex; all leases and the pool were disposed
         * before the queue-shaped source's final object destruction. */
        DESTROY_ATOMIC_HELPER_MUT(pThis->mutQueueSize);
        DESTROY_ATOMIC_HELPER_MUT(pThis->mutLogDeq);
        DESTROY_ATOMIC_HELPER_MUT(pThis->mutShutdownImmediate);
        DESTROY_ATOMIC_HELPER_MUT64(pThis->mutCtrMutexContention);
        DESTROY_ATOMIC_HELPER_MUT64(pThis->mutCtrMutexWaitNs);
        free(pThis->pszSpoolDir);
        FINALIZE;
    }
    DBGOPRINT((obj_t *)pThis, "shutdown: begin to destruct queue\n");
    /* The local FixedArray allocation and accepted-work bound are immutable. */
    if (!pThis->bLocalScope && ourConf->globals.shutdownQueueDoubleSize) {
        pThis->iHighWtrMrk *= 2;
        pThis->iMaxQueueSize *= 2;
    }
    if (pThis->bQueueStarted) {
        /* shut down all workers
         * We do not need to shutdown workers when we are in enqueue-only mode or we are a
         * direct queue - because in both cases we have none... ;)
         * with a child! -- rgerhards, 2008-01-28
         */
        if (pThis->qType != QUEUETYPE_DIRECT && !pThis->bEnqOnly && pThis->pqParent == NULL && pThis->pWtpReg != NULL)
            qqueueShutdownWorkers(pThis);

        /* Snapshot objects hold pointers into both BE and FE pools. Unlink
         * them under the stats-list lock before either pool can be destroyed. */
        qqueueLocalStatsDestruct(&pThis->localStats);

        /* Destroy the now-joined regular pool before inspecting the remaining
         * size or starting save-on-shutdown. It is no longer needed, and this
         * keeps the persistence phase's worker ownership explicit. */
        if (pThis->pWtpReg != NULL && (pThis->qType != QUEUETYPE_DIRECT || pThis->localGraphConf != NULL)) {
            wtpDestruct(&pThis->pWtpReg);
        }

        if (!pThis->localGraphStopped && pThis->bIsDA && getPhysicalQueueSize(pThis) > 0) {
            if (pThis->bSaveOnShutdown) {
                LogMsg(0, RS_RET_TIMED_OUT, LOG_INFO,
                       "%s: queue holds %d messages after shutdown of workers. "
                       "queue.saveonshutdown is set, so data will now be spooled to disk",
                       objGetName((obj_t *)pThis), getPhysicalQueueSize(pThis));
                CHKiRet(DoSaveOnShutdown(pThis));
            } else {
                LogMsg(0, RS_RET_TIMED_OUT, LOG_WARNING,
                       "%s: queue holds %d messages after shutdown of workers. "
                       "queue.saveonshutdown is NOT set, so data will be discarded.",
                       objGetName((obj_t *)pThis), getPhysicalQueueSize(pThis));
            }
        }

        /* finally destruct our (regular) worker thread pool
         * Note: currently pWtpReg is never NULL, but if we optimize our logic, this may happen,
         * e.g. when they are not created in enqueue-only mode. We already check the condition
         * as this may otherwise be very hard to find once we optimize (and have long forgotten
         * about this condition here ;)
         * rgerhards, 2008-01-25
         */
        /* Now check if we actually have a DA queue and, if so, destruct it.
         * The wtp must be destructed before the child queue. Shutdown has already
         * joined its workers, so destruction now releases only pool resources.
         * Please note that this also generates a situation
         * where it is possible that the DA queue has a parent pointer but the parent has
         * no WtpDA associated with it - which is perfectly legal thanks to this code here.
         */
        if (pThis->pWtpDA != NULL) {
            wtpDestruct(&pThis->pWtpDA);
        }
        if (pThis->pqDA != NULL) {
            qqueueDestruct(&pThis->pqDA);
        }
        if (pThis->localRetiredDA != NULL) qqueueDestruct(&pThis->localRetiredDA);

        /* persist the queue (we always do that - queuePersits() does cleanup if the queue is empty)
         * This handler is most important for disk queues, it will finally persist the necessary
         * on-disk structures. In theory, other queueing modes may implement their other (non-DA)
         * methods of persisting a queue between runs, but in practice all of this is done via
         * disk queues and DA mode. Anyhow, it doesn't hurt to know that we could extend it here
         * if need arises (what I doubt...) -- rgerhards, 2008-01-25
         */
        CHKiRet_Hdlr(qqueuePersist(pThis, QUEUE_NO_CHECKPOINT)) {
            DBGOPRINT((obj_t *)pThis, "error %d persisting queue - data lost!\n", iRet);
        }

        /* finally, clean up some simple things... */
        if (pThis->pqParent == NULL) {
            /* if we are not a child, we allocated our own mutex, which we now need to destroy */
            pthread_mutex_destroy(pThis->mut);
            free(pThis->mut);
        }
        pthread_mutex_destroy(&pThis->mutThrdMgmt);
        pthread_cond_destroy(&pThis->notFull);
        pthread_cond_destroy(&pThis->belowFullDlyWtrMrk);
        pthread_cond_destroy(&pThis->belowLightDlyWtrMrk);

        DESTROY_ATOMIC_HELPER_MUT(pThis->mutQueueSize);
        DESTROY_ATOMIC_HELPER_MUT(pThis->mutLogDeq);
        DESTROY_ATOMIC_HELPER_MUT(pThis->mutShutdownImmediate);
        DESTROY_ATOMIC_HELPER_MUT64(pThis->mutCtrMutexContention);
        DESTROY_ATOMIC_HELPER_MUT64(pThis->mutCtrMutexWaitNs);

        /* type-specific destructor */
        iRet = pThis->qDestruct(pThis);
    }

    free(pThis->pszFilePrefix);
    free(pThis->pszSpoolDir);
    if (pThis->useCryprov) {
        pThis->cryprov.Destruct(&pThis->cryprovData);
        obj.ReleaseObj(__FILE__, pThis->cryprovNameFull + 2, pThis->cryprovNameFull, (void *)&pThis->cryprov);
        free(pThis->cryprovName);
        free(pThis->cryprovNameFull);
    }

    /* some queues do not provide stats and thus have no statsobj! */
    if (pThis->statsobj != NULL) statsobj.Destruct(&pThis->statsobj);
    qqueueLocalDestruct(pThis);
ENDobjDestruct(qqueue)


/* set the queue's spool directory. The directory MUST NOT be NULL.
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it.
 */
rsRetVal qqueueSetSpoolDir(qqueue_t *pThis, uchar *pszSpoolDir, int lenSpoolDir) {
    DEFiRet;

    free(pThis->pszSpoolDir);
    CHKmalloc(pThis->pszSpoolDir = ustrdup(pszSpoolDir));
    pThis->lenSpoolDir = lenSpoolDir;

finalize_it:
    RETiRet;
}


/* set the queue's file prefix
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it.
 * rgerhards, 2008-01-09
 */
rsRetVal qqueueSetFilePrefix(qqueue_t *pThis, uchar *pszPrefix, size_t iLenPrefix) {
    DEFiRet;

    free(pThis->pszFilePrefix);
    pThis->pszFilePrefix = NULL;

    if (pszPrefix == NULL) /* just unset the prefix! */
        ABORT_FINALIZE(RS_RET_OK);

    if ((pThis->pszFilePrefix = malloc(iLenPrefix + 1)) == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    memcpy(pThis->pszFilePrefix, pszPrefix, iLenPrefix + 1);
    pThis->lenFilePrefix = iLenPrefix;

finalize_it:
    RETiRet;
}

/* set the queue's maximum file size
 * rgerhards, 2008-01-09
 */
rsRetVal qqueueSetMaxFileSize(qqueue_t *pThis, size_t iMaxFileSize) {
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);

    if (iMaxFileSize < 1024) {
        ABORT_FINALIZE(RS_RET_VALUE_TOO_LOW);
    }

    pThis->iMaxFileSize = iMaxFileSize;

finalize_it:
    RETiRet;
}


/* enqueue a single data object.
 * Note that the queue mutex MUST already be locked when this function is called.
 * rgerhards, 2009-06-16
 */
/* Caller holds the queue mutex. A graph shutdown wakes pre-existing waiters
 * after publishing one immutable monotonic action deadline to every node. */
static int graphEnqueueTimeout(const qqueue_t *const queue, const int configured) {
    if (!queue->localGraphDraining) return configured;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    const int64_t remaining = ((int64_t)queue->localGraphActionDeadline.tv_sec - now.tv_sec) * 1000 +
                              (queue->localGraphActionDeadline.tv_nsec - now.tv_nsec) / 1000000;
    if (remaining <= 0) return 0;
    const int bounded = remaining > INT_MAX ? INT_MAX : (int)remaining;
    return configured < 0 || bounded < configured ? bounded : configured;
}

static rsRetVal doEnqSingleObj(qqueue_t *pThis, flowControl_t flowCtlType, smsg_t *pMsg) {
    return doEnqSingleObjContext(pThis, flowCtlType, pMsg, 0);
}

/* A returned source reference is already accepted. Keep that per-call context
 * across waits; a queue-global bypass flag would also admit external producers
 * while pthread_cond_wait releases the mutex. Other queue policies are unchanged. */
static rsRetVal doEnqSingleObjContext(qqueue_t *pThis, flowControl_t flowCtlType, smsg_t *pMsg, int sourceRetry) {
    DEFiRet;
    int err;
    struct timespec t;

    STATSCOUNTER_INC(pThis->ctrEnqueued, pThis->mutCtrEnqueued);
    /* size.enqueued mirrors enqueued: counted on arrival, before discard checks,
     * so it represents inbound byte volume (rejected slice tracked by ctrFDscrd). */
    STATSCOUNTER_ADD(pThis->ctrSizeEnqueued, pThis->mutCtrSizeEnqueued, (uint64_t)pMsg->iLenRawMsg);
    if (pThis->localGraphClosed && !sourceRetry) goto graph_closed;
    if (qqueueLocalIsClosed(pThis)) ABORT_FINALIZE(RS_RET_FORCE_TERM);
    /* first check if we need to discard this message (which will cause CHKiRet() to exit)
     */
    CHKiRet(qqueueChkDiscardMsg(pThis, pThis->iQueueSize, pMsg));

    /* handle flow control
     * There are two different flow control mechanisms: basic and advanced flow control.
     * Basic flow control has always been implemented and protects the queue structures
     * in that it makes sure no more data is enqueued than the queue is configured to
     * support. Enhanced flow control is being added today. There are some sources which
     * can easily be stopped, e.g. a file reader. This is the case because it is unlikely
     * that blocking those sources will have negative effects (after all, the file is
     * continued to be written). Other sources can somewhat be blocked (e.g. the kernel
     * log reader or the local log stream reader): in general, nothing is lost if messages
     * from these sources are not picked up immediately. HOWEVER, they can not block for
     * an extended period of time, as this either causes message loss or - even worse - some
     * other bad effects (e.g. unresponsive system in respect to the main system log socket).
     * Finally, there are some (few) sources which can not be blocked at all. UDP syslog is
     * a prime example. If a UDP message is not received, it is simply lost. So we can't
     * do anything against UDP sockets that come in too fast. The core idea of advanced
     * flow control is that we take into account the different natures of the sources and
     * select flow control mechanisms that fit these needs. This also means, in the end
     * result, that non-blockable sources like UDP syslog receive priority in the system.
     * It's a side effect, but a good one ;) -- rgerhards, 2008-03-14
     */
    if (unlikely(pThis->takeFlowCtlFromMsg)) { /* recommendation is NOT to use this option */
        flowCtlType = pMsg->flowCtlType;
    }
    if (flowCtlType == eFLOWCTL_FULL_DELAY) {
        while (pThis->iQueueSize >= pThis->iFullDlyMrk && !glbl.GetGlobalInputTermState() &&
               !qqueueLocalIsClosed(pThis)) {
            /* We have a problem during shutdown if we block eternally. In that
             * case, the the input thread cannot be terminated. So we wake up
             * from time to time to check for termination.
             * TODO/v6(at earliest): check if we could signal the condition during
             * shutdown. However, this requires new queue registries and thus is
             * far to much change for a stable version (and I am still not sure it
             * is worth the effort, given how seldom this situation occurs and how
             * few resources the wakeups need). -- rgerhards, 2012-05-03
             * In any case, this was the old code (if we do the TODO):
             * pthread_cond_wait(&pThis->belowFullDlyWtrMrk, pThis->mut);
             */
            DBGOPRINT((obj_t *)pThis,
                      "doEnqSingleObject: FullDelay mark reached for full "
                      "delayable message - blocking, queue size is %d.\n",
                      pThis->iQueueSize);
            timeoutComp(&t, 1000);
            err = pthread_cond_timedwait(&pThis->belowLightDlyWtrMrk, pThis->mut, &t);
            if (err != 0 && err != ETIMEDOUT) {
                /* Something is really wrong now. Report to debug log and abort the
                 * wait. That keeps us running, even though we may lose messages.
                 */
                DBGOPRINT((obj_t *)pThis,
                          "potential program bug: pthread_cond_timedwait()"
                          "/fulldelay returned %d\n",
                          err);
                break;
            }
            DBGPRINTF("wti worker in full delay timed out, checking termination...\n");
        }
    } else if (flowCtlType == eFLOWCTL_LIGHT_DELAY && !glbl.GetGlobalInputTermState()) {
        if (pThis->iQueueSize >= pThis->iLightDlyMrk) {
            DBGOPRINT((obj_t *)pThis,
                      "doEnqSingleObject: LightDelay mark reached for light "
                      "delayable message - blocking a bit.\n");
            timeoutComp(&t, 1000); /* 1000 millisconds = 1 second TODO: make configurable */
            err = pthread_cond_timedwait(&pThis->belowLightDlyWtrMrk, pThis->mut, &t);
            if (err != 0 && err != ETIMEDOUT) {
                /* Something is really wrong now. Report to debug log */
                DBGOPRINT((obj_t *)pThis,
                          "potential program bug: pthread_cond_timedwait()"
                          "/lightdelay returned %d\n",
                          err);
            }
        }
    }

    /* from our regular flow control settings, we are now ready to enqueue the object.
     * However, we now need to do a check if the queue permits to add more data. If that
     * is not the case, basic flow control enters the field, which means we wait for
     * the queue to become ready or drop the new message. -- rgerhards, 2008-03-14
     */
    while ((pThis->iMaxQueueSize > 0 && pThis->iQueueSize >= pThis->iMaxQueueSize) ||
           ((pThis->qType == QUEUETYPE_DISK || pThis->bIsDA) && pThis->sizeOnDiskMax != 0 &&
            pThis->tVars.disk.sizeOnDisk > pThis->sizeOnDiskMax) ||
           (pThis->qType == QUEUETYPE_SEGMENTED_DISK && pThis->sizeOnDiskMax != 0 &&
            getQueueDiskBytes(pThis) >= pThis->sizeOnDiskMax)) {
        if (pThis->localGraphClosed && !sourceRetry) goto graph_closed;
        if (qqueueLocalIsClosed(pThis)) ABORT_FINALIZE(RS_RET_FORCE_TERM);
        STATSCOUNTER_INC(pThis->ctrFull, pThis->mutCtrFull);
        if (pThis->toEnq == 0 || pThis->bEnqOnly) {
            DBGOPRINT((obj_t *)pThis,
                      "doEnqSingleObject: queue FULL - configured for immediate "
                      "discarding QueueSize=%d MaxQueueSize=%d sizeOnDisk=%lld "
                      "sizeOnDiskMax=%lld\n",
                      pThis->iQueueSize, pThis->iMaxQueueSize, getQueueDiskBytes(pThis), pThis->sizeOnDiskMax);
            STATSCOUNTER_INC(pThis->ctrFDscrd, pThis->mutCtrFDscrd);
            msgDestruct(&pMsg);
            ABORT_FINALIZE(RS_RET_QUEUE_FULL);
        } else {
            DBGOPRINT((obj_t *)pThis, "doEnqSingleObject: queue FULL - waiting %dms to drain.\n", pThis->toEnq);
            /* Multi-submit and single-message enqueue normally advise workers
             * only after doEnqSingleObj() returns. If this call blocks first,
             * an already-idle worker would never be awakened to make room. */
            qqueueAdviseMaxWorkers(pThis);
            if (pThis->localGraphConf == NULL && glbl.GetGlobalInputTermState()) {
                DBGOPRINT((obj_t *)pThis,
                          "doEnqSingleObject: queue FULL, discard due to "
                          "FORCE_TERM.\n");
                ABORT_FINALIZE(RS_RET_FORCE_TERM);
            }
            timeoutComp(&t, sourceRetry ? pThis->toEnq : graphEnqueueTimeout(pThis, pThis->toEnq));
            const int r = pthread_cond_timedwait(&pThis->notFull, pThis->mut, &t);
            if (dbgTimeoutToStderr && r != 0) {
                fprintf(stderr,
                        "%lld: queue timeout(%dms), error %d%s, "
                        "lost message %s\n",
                        (long long)time(NULL), pThis->toEnq, r, (r == ETIMEDOUT) ? "[ETIMEDOUT]" : "", pMsg->pszRawMsg);
            }
            if (r == ETIMEDOUT) {
                DBGOPRINT((obj_t *)pThis, "doEnqSingleObject: cond timeout, dropping message!\n");
                STATSCOUNTER_INC(pThis->ctrFDscrd, pThis->mutCtrFDscrd);
                msgDestruct(&pMsg);
                ABORT_FINALIZE(RS_RET_QUEUE_FULL);
            } else if (r != 0) {
                DBGOPRINT((obj_t *)pThis, "doEnqSingleObject: cond error %d, dropping message!\n", r);
                STATSCOUNTER_INC(pThis->ctrFDscrd, pThis->mutCtrFDscrd);
                msgDestruct(&pMsg);
                ABORT_FINALIZE(RS_RET_QUEUE_FULL);
            }
            dbgoprint((obj_t *)pThis, "doEnqSingleObject: wait solved queue full condition, enqueing\n");
        }
    }

    /* Closure leaves this reference with the local wrapper, which also
     * consumes any unprocessed suffix. Global ownership remains unchanged. */
    if (pThis->localGraphClosed && !sourceRetry) goto graph_closed;
    if (qqueueLocalIsClosed(pThis)) ABORT_FINALIZE(RS_RET_FORCE_TERM);
    /* and finally enqueue the message */
    CHKiRet(qqueueAdd(pThis, pMsg));
    /* Queue writers are serialized by pThis->mut, but impstats reads this
     * legacy max-size counter independently. Keep the reader and this write
     * atomic without changing the generic set-max helper's contract. */
    if (STATSCOUNTER_ENABLED()) {
        const int queue_size = PREFER_LOAD_INT(&pThis->iQueueSize);
        if (queue_size > PREFER_LOAD_INT(&pThis->ctrMaxqsize)) {
            PREFER_STORE_INT(&pThis->ctrMaxqsize, queue_size);
        }
    }

    /* check if we had a file rollover and need to persist
     * the .qi file for robustness reasons.
     * Note: the n=2 write is required for closing the old file and
     * the n=1 write is required after opening and writing to the new
     * file.
     */
    if (pThis->tVars.disk.nForcePersist > 0) {
        DBGOPRINT((obj_t *)pThis, ".qi file write required for robustness reasons (n=%d)\n",
                  pThis->tVars.disk.nForcePersist);
        pThis->tVars.disk.nForcePersist--;
        qqueuePersist(pThis, QUEUE_CHECKPOINT);
    }

    FINALIZE;
graph_closed:
    /* Global graph boundaries consume rejected refs like their ordinary full
     * queue path. Internal diagnostics may arrive after upstream workers stop. */
    STATSCOUNTER_INC(pThis->ctrFDscrd, pThis->mutCtrFDscrd);
    msgDestruct(&pMsg);
    iRet = RS_RET_QUEUE_FULL;
finalize_it:
    RETiRet;
}

/* The local wrapper has a total reference-consumption contract. Queue full
 * already consumed the current reference; FORCE_TERM did not. A fixed-array
 * insertion cannot otherwise fail after the checked local activation gate. */
rsRetVal qqueueLocalSubmitBackend(qqueue_t *const owner,
                                  smsg_t *const *const messages,
                                  const size_t count,
                                  const int singleFlowControl,
                                  const enum qqueueLocalRouteReason reason) {
    rsRetVal result = RS_RET_OK;
    pthread_mutex_lock(owner->mut);
    qqueueLocalBackendBegin(owner);
    qqueueLocalBackendAttempt(owner, count);
    qqueueLocalBackendRoute(owner, count, reason);
    for (size_t i = 0; i < count; ++i) {
        const flowControl_t flow = singleFlowControl < 0 ? messages[i]->flowCtlType : (flowControl_t)singleFlowControl;
        const rsRetVal ret = doEnqSingleObj(owner, flow, messages[i]);
        if (ret == RS_RET_OK) {
            qqueueLocalBackendAdmitted(owner, 1);
            /* A later element may block on capacity: publish each accepted prefix. */
            qqueueLocalWakeBackendHelpers(owner);
        } else if (ret == RS_RET_QUEUE_FULL) {
            qqueueLocalBackendRejected(owner, 1);
        } else {
            /* With FixedArray and disabled sampling/severity discard, errors
             * reaching here leave the current reference with this wrapper. */
            qqueueLocalBackendRejected(owner, count - i);
            uint64_t tailBytes = 0;
            for (size_t tail = i + 1; tail < count; ++tail) tailBytes += (uint64_t)messages[tail]->iLenRawMsg;
            STATSCOUNTER_ADD(owner->ctrEnqueued, owner->mutCtrEnqueued, count - i - 1);
            STATSCOUNTER_ADD(owner->ctrSizeEnqueued, owner->mutCtrSizeEnqueued, tailBytes);
            pthread_mutex_unlock(owner->mut);
            for (size_t tail = i; tail < count; ++tail) {
                smsg_t *message = messages[tail];
                msgDestruct(&message);
            }
            pthread_mutex_lock(owner->mut);
            result = ret;
            break;
        }
    }
    if (!qqueueLocalIsClosed(owner)) qqueueAdviseMaxWorkers(owner);
    qqueueLocalBackendEnd(owner);
    pthread_mutex_unlock(owner->mut);
    return result;
}

/* Already-accepted transfer: failure retains the exact supplied reference.
 * Callers hold no FE mutex, and may transfer only after taking its consumer
 * endpoint through producer quiescence and worker join. */
rsRetVal qqueueLocalTransferBackend(qqueue_t *const owner,
                                    smsg_t *const message,
                                    const struct timespec *const deadline) {
    rsRetVal ret = RS_RET_OK;
    pthread_mutex_lock(owner->mut);
    if (owner->qType != QUEUETYPE_FIXED_ARRAY || owner->local == NULL ||
        (qqueueLocalIsClosed(owner) && !owner->localDASaving)) {
        ret = RS_RET_FORCE_TERM;
        goto done;
    }
    if (owner->localDASaving) {
        while (getPhysicalQueueSize(owner) >= owner->iMaxQueueSize) {
            pthread_mutex_unlock(owner->mut);
            ret = qqueueSaveLocalBackend(owner);
            pthread_mutex_lock(owner->mut);
            if (ret != RS_RET_OK || getPhysicalQueueSize(owner) >= owner->iMaxQueueSize) {
                ret = ret == RS_RET_OK ? RS_RET_QUEUE_FULL : ret;
                goto done;
            }
        }
        ret = qqueueAdd(owner, message);
        goto done;
    }
    struct timespec now;
    if (deadline == NULL || clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec > deadline->tv_sec ||
        (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec)) {
        ret = RS_RET_TIMED_OUT;
        goto done;
    }
    while (owner->iQueueSize >= owner->iMaxQueueSize) {
        qqueueAdviseMaxWorkers(owner);
        if (deadline == NULL || qqueueLocalBackendWaitSpace(owner, deadline) != 0 || qqueueLocalIsClosed(owner)) {
            ret = RS_RET_TIMED_OUT;
            goto done;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec > deadline->tv_sec ||
        (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec)) {
        ret = RS_RET_TIMED_OUT;
        goto done;
    }
    ret = qqueueAdd(owner, message);
    if (ret == RS_RET_OK) qqueueAdviseMaxWorkers(owner);
done:
    pthread_mutex_unlock(owner->mut);
    return ret;
}

void qqueueLocalDiscardBackend(qqueue_t *const owner) {
    pthread_mutex_lock(owner->mut);
    assert(owner->nLogDeq == 0);
    while (owner->iQueueSize > 0) {
        smsg_t *message = NULL;
        qDeqFixedArray(owner, &message);
        qDelFixedArray(owner);
        ATOMIC_DEC(&owner->iQueueSize, &owner->mutQueueSize);
        qqueueSubtractOverallQueueSize(1);
        qqueueLocalBackendTerminal(owner, 1, 1);
        /* No workers or external admissions remain; final destruction need
         * not hold the BE mutex and can emit an internal diagnostic safely. */
        pthread_mutex_unlock(owner->mut);
        msgDestruct(&message);
        pthread_mutex_lock(owner->mut);
    }
    pthread_mutex_unlock(owner->mut);
}

/* ------------------------------ multi-enqueue functions ------------------------------ */
/* enqueue multiple user data elements at once. The aim is to provide a faster interface
 * for object submission. Uses the multi_submit_t helper object.
 * Please note that this function is not cancel-safe and consequently
 * sets the calling thread's cancelibility state to PTHREAD_CANCEL_DISABLE
 * during its execution. If that is not done, race conditions occur if the
 * thread is canceled (most important use case is input module termination).
 * rgerhards, 2009-06-16
 * Note: there now exists multiple different functions implementing specially
 * optimized algorithms for different config cases. -- rgerhards, 2010-06-09
 */
/* now the function for all modes but direct */
static rsRetVal qqueueMultiEnqObjNonDirect(qqueue_t *pThis, multi_submit_t *pMultiSub) {
    int iCancelStateSave;
    int i;
    rsRetVal localRet;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    assert(pMultiSub != NULL);
    if (pThis->local != NULL) return qqueueLocalSubmit(pThis, pMultiSub->ppMsgs, (size_t)pMultiSub->nElem, -1);

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
    qqueueLock(pThis);
    for (i = 0; i < pMultiSub->nElem; ++i) {
        localRet = doEnqSingleObj(pThis, pMultiSub->ppMsgs[i]->flowCtlType, (void *)pMultiSub->ppMsgs[i]);
        if (localRet != RS_RET_OK && localRet != RS_RET_QUEUE_FULL) ABORT_FINALIZE(localRet);
    }
    qqueueChkPersist(pThis, pMultiSub->nElem);

finalize_it:
    /* make sure at least one worker is running. */
    qqueueAdviseMaxWorkers(pThis);
    /* and release the mutex */
    d_pthread_mutex_unlock(pThis->mut);
    pthread_setcancelstate(iCancelStateSave, NULL);
    DBGOPRINT((obj_t *)pThis, "MultiEnqObj advised worker start\n");

    RETiRet;
}

/* now, the same function, but for direct mode */
static rsRetVal qqueueMultiEnqObjDirect(qqueue_t *pThis, multi_submit_t *pMultiSub) {
    int i;
    wti_t *pWti;
    DEFiRet;

    pWti = wtiGetDummy();
    qqueueSetWtiShutdownImmediate(pThis, pWti);

    for (i = 0; i < pMultiSub->nElem; ++i) {
        CHKiRet(qAddDirectWithWti(pThis, (void *)pMultiSub->ppMsgs[i], pWti));
    }

finalize_it:
    RETiRet;
}
/* ------------------------------ END multi-enqueue functions ------------------------------ */


/* enqueue a new user data element
 * Enqueues the new element and awakes worker thread.
 */
rsRetVal qqueueEnqMsg(qqueue_t *pThis, flowControl_t flowCtlType, smsg_t *pMsg) {
    DEFiRet;
    int iCancelStateSave;
    ISOBJ_TYPE_assert(pThis, qqueue);

    if (pThis->local != NULL) return qqueueLocalSubmit(pThis, &pMsg, 1, (int)flowCtlType);

    const int isNonDirectQ = pThis->qType != QUEUETYPE_DIRECT;

    if (isNonDirectQ) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
        qqueueLock(pThis);
    }

    CHKiRet(doEnqSingleObj(pThis, flowCtlType, pMsg));

    qqueueChkPersist(pThis, 1);

finalize_it:
    if (isNonDirectQ) {
        /* make sure at least one worker is running. */
        qqueueAdviseMaxWorkers(pThis);
        /* and release the mutex */
        d_pthread_mutex_unlock(pThis->mut);
        pthread_setcancelstate(iCancelStateSave, NULL);
        DBGOPRINT((obj_t *)pThis, "EnqueueMsg advised worker start\n");
    }

    RETiRet;
}


/* Called even when conversion reports that no usable queue parameter exists. */
void qqueueNoteLocalConfigIntent(struct nvlst *const lst) {
    if (loadConf == NULL) return;
    for (struct nvlst *nv = lst; nv != NULL; nv = nv->next) {
        if (nv->name == NULL) continue;
        if (!es_strcasebufcmp(nv->name, (uchar *)"queue.scope", 11)) {
            if (nv->val.datatype != 'S') {
                loadConf->bLocalConfigRequested = loadConf->bLocalConfigError = 1;
            } else if (es_strcasebufcmp(nv->val.d.estr, (uchar *)"global", 6)) {
                loadConf->bLocalConfigRequested = 1;
                if (es_strcasebufcmp(nv->val.d.estr, (uchar *)"local", 5)) loadConf->bLocalConfigError = 1;
            }
        } else if (es_strlen(nv->name) >= 12 &&
                   !strncasecmp((const char *)es_getBufAddr(nv->name), "queue.local.", 12)) {
            loadConf->bLocalConfigRequested = 1;
        }
    }
}

/* are any queue params set at all? 1 - yes, 0 - no
 * We need to evaluate the param block for this function, which is somewhat
 * inefficient. HOWEVER, this is only done during config load, so we really
 * don't care... -- rgerhards, 2013-05-10
 */
int queueCnfParamsSet(struct nvlst *lst) {
    int r;
    struct cnfparamvals *pvals;

    qqueueNoteLocalConfigIntent(lst);
    pvals = nvlstGetParams(lst, &pblk, NULL);
    r = cnfparamvalsIsSet(&pblk, pvals);
    cnfparamvalsDestruct(pvals, &pblk);
    return r;
}


static rsRetVal initCryprov(qqueue_t *pThis, struct nvlst *lst) {
    uchar szDrvrName[1024];
    DEFiRet;

    if (snprintf((char *)szDrvrName, sizeof(szDrvrName), "lmcry_%s", pThis->cryprovName) == sizeof(szDrvrName)) {
        LogError(0, RS_RET_ERR,
                 "queue: crypto provider "
                 "name is too long: '%s' - encryption disabled",
                 pThis->cryprovName);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pThis->cryprovNameFull = ustrdup(szDrvrName);

    pThis->cryprov.ifVersion = cryprovCURR_IF_VERSION;
    /* The pDrvrName+2 below is a hack to obtain the object name. It
     * safes us to have yet another variable with the name without "lm" in
     * front of it. If we change the module load interface, we may re-think
     * about this hack, but for the time being it is efficient and clean enough.
     */
    if (obj.UseObj(__FILE__, szDrvrName, szDrvrName, (void *)&pThis->cryprov) != RS_RET_OK) {
        LogError(0, RS_RET_LOAD_ERROR,
                 "queue: could not load "
                 "crypto provider '%s' - encryption disabled",
                 szDrvrName);
        ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
    }

    if (pThis->cryprov.Construct(&pThis->cryprovData) != RS_RET_OK) {
        LogError(0, RS_RET_CRYPROV_ERR,
                 "queue: error constructing "
                 "crypto provider %s dataset - encryption disabled",
                 szDrvrName);
        ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
    }
    CHKiRet(pThis->cryprov.SetCnfParam(pThis->cryprovData, lst, CRYPROV_PARAMTYPE_DISK));

    dbgprintf("loaded crypto provider %s, data instance at %p\n", szDrvrName, pThis->cryprovData);
    pThis->useCryprov = 1;
finalize_it:
    RETiRet;
}

/* check the queue file name is unique. */
static rsRetVal ATTR_NONNULL() checkUniqueDiskFile(qqueue_t *const pThis) {
    DEFiRet;
    struct queue_filename *queue_fn_curr = queue_filename_root;
    struct queue_filename *newetry = NULL;
    const char *const curr_dirname = (pThis->pszSpoolDir == NULL) ? "" : (char *)pThis->pszSpoolDir;

    if (pThis->pszFilePrefix == NULL) {
        FINALIZE; /* no disk queue! */
    }

    while (queue_fn_curr != NULL) {
        if (!strcmp((const char *)pThis->pszFilePrefix, queue_fn_curr->filename) &&
            !strcmp(curr_dirname, queue_fn_curr->dirname)) {
            parser_errmsg(
                "queue directory '%s' and file name prefix '%s' already used. "
                "This is not possible. Please make it unique.",
                curr_dirname, pThis->pszFilePrefix);
            ABORT_FINALIZE(RS_RET_ERR_QUEUE_FN_DUP);
        }
        queue_fn_curr = queue_fn_curr->next;
    }

    /* name ok, so let's add it to the list */
    CHKmalloc(newetry = calloc(1, sizeof(struct queue_filename)));
    CHKmalloc(newetry->filename = strdup((char *)pThis->pszFilePrefix));
    CHKmalloc(newetry->dirname = strdup(curr_dirname));
    newetry->next = queue_filename_root;
    queue_filename_root = newetry;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (newetry != NULL) {
            free((void *)newetry->filename);
            free((void *)newetry->dirname);
            free((void *)newetry);
        }
    }
    RETiRet;
}

void qqueueCorrectParams(qqueue_t *pThis) {
    int goodval; /* a "good value" to use for comparisons (different objects) */
    int needWarnHigh = 0, needWarnLow = 0;

    if (pThis->iMaxQueueSize < 100 && (pThis->qType == QUEUETYPE_LINKEDLIST || pThis->qType == QUEUETYPE_FIXED_ARRAY)) {
        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
               "Note: queue.size=\"%d\" is very "
               "low and can lead to unpredictable results. See also "
               "https://www.rsyslog.com/lower-bound-for-queue-sizes/",
               pThis->iMaxQueueSize);
    }

    /* we need to do a quick check if our water marks are set plausible. If not,
     * we correct the most important shortcomings.
     */
    goodval = (pThis->iMaxQueueSize / 100) * 60;
    if (pThis->iHighWtrMrk != -1 && pThis->iHighWtrMrk < goodval) {
        LogMsg(0, RS_RET_CONF_PARSE_WARNING, LOG_WARNING,
               "queue \"%s\": high water mark "
               "is set quite low at %d. You should only set it below "
               "60%% (%d) if you have a good reason for this.",
               obj.GetName((obj_t *)pThis), pThis->iHighWtrMrk, goodval);
    }

    if (pThis->iNumWorkerThreads > 1) {
        goodval = (pThis->iMaxQueueSize / 100) * 10;
        if (pThis->iMinMsgsPerWrkr != -1 && pThis->iMinMsgsPerWrkr < goodval) {
            LogMsg(0, RS_RET_CONF_PARSE_WARNING, LOG_WARNING,
                   "queue \"%s\": "
                   "queue.workerThreadMinimumMessage "
                   "is set quite low at %d. You should only set it below "
                   "10%% (%d) if you have a good reason for this.",
                   obj.GetName((obj_t *)pThis), pThis->iMinMsgsPerWrkr, goodval);
        }
    }

    if (pThis->iDiscardMrk > pThis->iMaxQueueSize) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "error: queue \"%s\": "
                 "queue.discardMark %d is set larger than queue.size",
                 obj.GetName((obj_t *)pThis), pThis->iDiscardMrk);
    }

    goodval = (pThis->iMaxQueueSize / 100) * 80;
    if (pThis->iDiscardMrk != -1 && pThis->iDiscardMrk < goodval) {
        LogMsg(0, RS_RET_CONF_PARSE_WARNING, LOG_WARNING,
               "queue \"%s\": queue.discardMark "
               "is set quite low at %d. You should only set it below "
               "80%% (%d) if you have a good reason for this.",
               obj.GetName((obj_t *)pThis), pThis->iDiscardMrk, goodval);
    }

    if (pThis->pszFilePrefix != NULL) { /* This means we have a potential DA queue */
        if (pThis->iFullDlyMrk != -1 && pThis->iFullDlyMrk < pThis->iHighWtrMrk) {
            LogMsg(0, RS_RET_CONF_WRN_FULLDLY_BELOW_HIGHWTR, LOG_WARNING,
                   "queue \"%s\": queue.fullDelayMark "
                   "is set below high water mark. This will result in DA mode "
                   " NOT being activated for full delayable messages: In many "
                   "cases this is a configuration error, please check if this "
                   "is really what you want",
                   obj.GetName((obj_t *)pThis));
        }
    }

    /* now come parameter corrections and defaults */
    if (pThis->iHighWtrMrk != -1 && (pThis->iHighWtrMrk < 2 || pThis->iHighWtrMrk > pThis->iMaxQueueSize)) {
        needWarnHigh = 1;
    }

    if (pThis->iHighWtrMrk == -1 || needWarnHigh) {
        pThis->iHighWtrMrk = (pThis->iMaxQueueSize / 100) * 90;
        if (pThis->iHighWtrMrk == 0) { /* guard against very low max queue sizes! */
            pThis->iHighWtrMrk = pThis->iMaxQueueSize;
        }
        if (needWarnHigh) {
            LogMsg(0, RS_RET_CONF_PARSE_WARNING, LOG_WARNING,
                   "queue \"%s\": queue.highWaterMark "
                   "is invalid (must be between 2 and queue size). It has been automatically "
                   "adjusted to %d. In any case, we strongly recommend to review the "
                   "queue definition and resolve inconsistencies to guarantee "
                   "the config really matches your intent.",
                   obj.GetName((obj_t *)pThis), pThis->iHighWtrMrk);
        }
    }

    if (pThis->iLowWtrMrk != -1 && (pThis->iLowWtrMrk < 2 || pThis->iLowWtrMrk > pThis->iHighWtrMrk)) {
        needWarnLow = 1;
    }

    if (pThis->iLowWtrMrk == -1 || needWarnLow) {
        pThis->iLowWtrMrk = (pThis->iHighWtrMrk * 7ll / 10);
        if (pThis->iLowWtrMrk == 0) {
            pThis->iLowWtrMrk = 1;
        }
        if (needWarnLow) {
            LogMsg(0, RS_RET_CONF_PARSE_WARNING, LOG_WARNING,
                   "queue \"%s\": queue.lowWaterMark "
                   "is invalid (must be between 2 and highWaterMark). It has been automatically "
                   "adjusted to %d. In any case, we strongly recommend to review the "
                   "queue definition and resolve inconsistencies to guarantee "
                   "the config really matches your intent.",
                   obj.GetName((obj_t *)pThis), pThis->iLowWtrMrk);
        }
    }

    if ((pThis->iMinMsgsPerWrkr < 1 || pThis->iMinMsgsPerWrkr > pThis->iMaxQueueSize)) {
        pThis->iMinMsgsPerWrkr = pThis->iMaxQueueSize / pThis->iNumWorkerThreads;
    }

    if (pThis->iFullDlyMrk == -1 || pThis->iFullDlyMrk > pThis->iMaxQueueSize) {
        pThis->iFullDlyMrk = (pThis->iMaxQueueSize / 100) * 97;
        if (pThis->iFullDlyMrk == 0) {
            pThis->iFullDlyMrk = (pThis->iMaxQueueSize == 1) ? 1 : pThis->iMaxQueueSize - 1;
        }
    }

    if (pThis->iLightDlyMrk == 0) {
        pThis->iLightDlyMrk = pThis->iMaxQueueSize;
    }

    if (pThis->iLightDlyMrk == -1 || pThis->iLightDlyMrk > pThis->iMaxQueueSize) {
        pThis->iLightDlyMrk = (pThis->iMaxQueueSize / 100) * 70;
        if (pThis->iLightDlyMrk == 0) {
            pThis->iLightDlyMrk = (pThis->iMaxQueueSize == 1) ? 1 : pThis->iMaxQueueSize - 1;
        }
    }

    if (pThis->iDiscardMrk < 1 || pThis->iDiscardMrk > pThis->iMaxQueueSize) {
        pThis->iDiscardMrk = (pThis->iMaxQueueSize / 100) * 98;
        if (pThis->iDiscardMrk == 0) {
            /* for very small queues, we disable this by default */
            pThis->iDiscardMrk = pThis->iMaxQueueSize;
        }
    }

    if (pThis->iMaxQueueSize > 0 && pThis->iDeqBatchSize > pThis->iMaxQueueSize) {
        pThis->iDeqBatchSize = pThis->iMaxQueueSize;
    }
}

/* Preserve raw intent even if parameter conversion rejects or drops a value.
 * The ordinal mask refers to this immutable config descriptor table, not storage
 * layout. This cold path has no effect on ordinary global queue admission. */
static void qqueueNoteLocalParams(qqueue_t *const pThis, struct nvlst *const lst) {
    assert(pblk.nParams <= 64);
    for (struct nvlst *nv = lst; nv != NULL; nv = nv->next) {
        if (nv->name == NULL) continue;
        for (int i = 0; i < pblk.nParams; ++i) {
            if (!es_strcasebufcmp(nv->name, (uchar *)pblk.descr[i].name, strlen(pblk.descr[i].name))) {
                pThis->localExplicitParams |= UINT64_C(1) << i;
                if (!strcmp(pblk.descr[i].name, "queue.scope")) {
                    if (nv->val.datatype != 'S' || es_strcasebufcmp(nv->val.d.estr, (uchar *)"global", 6)) {
                        loadConf->bLocalConfigRequested = 1;
                        if (nv->val.datatype == 'S' && !es_strcasebufcmp(nv->val.d.estr, (uchar *)"local", 5))
                            pThis->bLocalScope = 1;
                    }
                } else if (!strncmp(pblk.descr[i].name, "queue.local.", 12)) {
                    loadConf->bLocalConfigRequested = 1;
                }
                break;
            }
        }
    }
}

static int qqueueLocalParamExplicit(const qqueue_t *const pThis, const char *const name) {
    const int idx = cnfparamGetIdx(&pblk, name);
    return idx >= 0 && (pThis->localExplicitParams & (UINT64_C(1) << idx)) != 0;
}

/* Validate before corrections can hide invalid values, and again at the shared
 * graph gate. Only that gate may mark bLocalConfigValidated. */
rsRetVal qqueueValidateLocalConfig(qqueue_t *const pThis) {
    static const char *const unsupported[] = {"queue.mindequeuebatchsize.timeout", "queue.discardmark",
                                              "queue.dequeuetimebegin", "queue.dequeuetimeend"};
    if (!pThis->bLocalScope) {
        if (pThis->bLocalConfigError || qqueueLocalParamExplicit(pThis, "queue.local.frontendsize") ||
            qqueueLocalParamExplicit(pThis, "queue.local.maxfrontends") ||
            qqueueLocalParamExplicit(pThis, "queue.local.frontendstats") ||
            qqueueLocalParamExplicit(pThis, "queue.local.helperbatchsize"))
            goto invalid;
        return RS_RET_OK;
    }
    if (pThis->bLocalConfigError || pThis->qType != QUEUETYPE_FIXED_ARRAY || pThis->iMaxQueueSize <= 0 ||
        pThis->iDeqBatchSize <= 0 || pThis->iNumWorkerThreads <= 0 || pThis->localFrontendSize <= 0 ||
        pThis->localMaxFrontends <= 0 || !qqueueLocalParamExplicit(pThis, "queue.local.frontendsize") ||
        !qqueueLocalParamExplicit(pThis, "queue.local.maxfrontends") || pThis->iMinDeqBatchSize != 0 ||
        pThis->iDiscardSeverity != 8 || pThis->iSmpInterval != 0 || pThis->iDeqSlowdown != 0 ||
        pThis->iDeqtWinFromHr != 0 || pThis->iDeqtWinToHr != 25 || pThis->toQShutdown < 0 || pThis->toActShutdown < 0 ||
        pThis->toEnq < 0 || pThis->toWrkShutdown < 0 || pThis->iFullDlyMrk < -1 || pThis->iFullDlyMrk == 0 ||
        pThis->iFullDlyMrk > pThis->iMaxQueueSize || pThis->iLightDlyMrk < -1 ||
        pThis->iLightDlyMrk > pThis->iMaxQueueSize)
        goto invalid;
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
        if (qqueueLocalParamExplicit(pThis, unsupported[i])) {
            parser_errmsg("local queue '%s': unsupported local queue parameter '%s'", objGetName((obj_t *)pThis),
                          unsupported[i]);
            goto invalid;
        }
    }
    /* B + N*(F+D): BE physical size includes active entries; FE active holdings
     * are separately bounded by D. Ensure both accounting and pointer buffers fit. */
    const uint64_t f = (uint64_t)pThis->localFrontendSize;
    const uint64_t d =
        (uint64_t)((pThis->iDeqBatchSize < pThis->localFrontendSize) ? pThis->iDeqBatchSize : pThis->localFrontendSize);
    if (pThis->localHelperBatchSizeSet &&
        (pThis->localHelperBatchSize < 0 || (uint64_t)pThis->localHelperBatchSize > d))
        goto invalid;
    const uint64_t n = (uint64_t)pThis->localMaxFrontends;
    if (n > (UINT64_MAX - (uint64_t)pThis->iMaxQueueSize) / (f + d) || f > SIZE_MAX / sizeof(smsg_t *) ||
        n > SIZE_MAX / (f * sizeof(smsg_t *)))
        goto invalid;
    return RS_RET_OK;
invalid:
    pThis->bLocalConfigError = 1;
    if (loadConf != NULL) loadConf->bLocalConfigError = 1;
    parser_errmsg("local queue '%s': configuration is outside the experimental local queue contract",
                  objGetName((obj_t *)pThis));
    return RS_RET_LOCAL_QUEUE_CONFIG;
}

/* apply all params from param block to queue. Must be called before
 * finalizing. This supports the v6 config system. Defaults were already
 * set during queue creation. The pvals object is destructed by this
 * function.
 */
rsRetVal qqueueApplyCnfParam(qqueue_t *pThis, struct nvlst *lst) {
    int i;
    struct cnfparamvals *pvals;
    int n_params_set = 0;
    DEFiRet;

    qqueueNoteLocalConfigIntent(lst);
    qqueueNoteLocalParams(pThis, lst);
    /* Facility-name conversion returns int before populating pvals. For S2's
     * disabled severity policy, qualify the raw value instead of accepting an
     * oversized number that that historical conversion can wrap into eight. */
    if (pThis->bLocalScope) {
        for (const struct nvlst *nv = lst; nv != NULL; nv = nv->next) {
            if (nv->name != NULL && !es_strcasebufcmp(nv->name, (uchar *)"queue.discardseverity", 21) &&
                (nv->val.datatype != 'S' || es_strbufcmp(nv->val.d.estr, (uchar *)"8", 1))) {
                pThis->bLocalConfigError = 1;
                loadConf->bLocalConfigError = 1;
            }
        }
    }
    pvals = nvlstGetParams(lst, &pblk, NULL);
    if (pvals == NULL) {
        parser_errmsg("error processing queue config parameters");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    if (Debug) {
        dbgprintf("queue param blk:\n");
        cnfparamsPrint(&pblk, pvals);
    }
    for (i = 0; i < pblk.nParams; ++i) {
        if (!pvals[i].bUsed) {
            if ((pThis->localExplicitParams & (UINT64_C(1) << i)) && loadConf->bLocalConfigRequested)
                pThis->bLocalConfigError = 1;
            continue;
        }
        n_params_set++;
        /* Inspect the wide source value before any legacy int assignment can
         * wrap into an allowed S2 zero/disabled/default value. Keep processing
         * scope itself so invalid local configuration remains identifiable. */
        if (pThis->bLocalScope && pvals[i].val.datatype == 'N' && strcmp(pblk.descr[i].name, "queue.maxdiskspace") &&
            strcmp(pblk.descr[i].name, "queue.maxfilesize") &&
            (pvals[i].val.d.n < INT_MIN || pvals[i].val.d.n > INT_MAX)) {
            pThis->bLocalConfigError = 1;
            loadConf->bLocalConfigError = 1;
            continue;
        }
        if (!strcmp(pblk.descr[i].name, "queue.scope")) {
            if (!es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"local", 5)) {
                pThis->bLocalScope = 1;
            } else if (!es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"global", 6)) {
                pThis->bLocalScope = 0;
            } else {
                pThis->bLocalConfigError = 1;
                loadConf->bLocalConfigError = 1;
                parser_errmsg("queue.scope must be 'global' or experimental 'local'");
            }
        } else if (!strcmp(pblk.descr[i].name, "queue.local.frontendsize")) {
            if (pvals[i].val.d.n <= 0 || pvals[i].val.d.n > INT_MAX)
                pThis->bLocalConfigError = 1;
            else
                pThis->localFrontendSize = (int)pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.local.helperbatchsize")) {
            pThis->localHelperBatchSizeSet = 1;
            if (pvals[i].val.d.n < 0 || pvals[i].val.d.n > INT_MAX)
                pThis->bLocalConfigError = 1;
            else
                pThis->localHelperBatchSize = (int)pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.local.frontendstats")) {
            pThis->localFrontendStats = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.local.maxfrontends")) {
            if (pvals[i].val.d.n <= 0 || pvals[i].val.d.n > INT_MAX)
                pThis->bLocalConfigError = 1;
            else
                pThis->localMaxFrontends = (int)pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.filename")) {
            CHKmalloc(pThis->pszFilePrefix = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
            pThis->lenFilePrefix = es_strlen(pvals[i].val.d.estr);
        } else if (!strcmp(pblk.descr[i].name, "queue.cry.provider")) {
            CHKmalloc(pThis->cryprovName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(pblk.descr[i].name, "queue.spooldirectory")) {
            free(pThis->pszSpoolDir);
            CHKmalloc(pThis->pszSpoolDir = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
            pThis->lenSpoolDir = es_strlen(pvals[i].val.d.estr);
            CHKiRet(validateQueueSpoolDir(pThis));
            if (pThis->lenSpoolDir > 0 && pThis->pszSpoolDir[pThis->lenSpoolDir - 1] == '/') {
                pThis->pszSpoolDir[pThis->lenSpoolDir - 1] = '\0';
                --pThis->lenSpoolDir;
                parser_errmsg(
                    "queue.spooldirectory must not end with '/', "
                    "corrected to '%s'",
                    pThis->pszSpoolDir);
            }
        } else if (!strcmp(pblk.descr[i].name, "queue.size")) {
            if (pvals[i].val.d.n > 0x7fffffff) {
                parser_warnmsg(
                    "queue.size higher than maximum (2147483647) - "
                    "corrected to maximum");
                pvals[i].val.d.n = 0x7fffffff;
            } else if (pvals[i].val.d.n > OVERSIZE_QUEUE_WATERMARK) {
                parser_warnmsg(
                    "queue.size=%d is very large - is this "
                    "really intended? More info at "
                    "https://www.rsyslog.com/avoid-overly-large-in-memory-queues/",
                    (int)pvals[i].val.d.n);
            }
            pThis->iMaxQueueSize = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.dequeuebatchsize")) {
            pThis->iDeqBatchSize = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.mindequeuebatchsize")) {
            pThis->iMinDeqBatchSize = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.mindequeuebatchsize.timeout")) {
            pThis->toMinDeqBatchSize = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.maxdiskspace")) {
            pThis->sizeOnDiskMax = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.highwatermark")) {
            pThis->iHighWtrMrk = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.lowwatermark")) {
            pThis->iLowWtrMrk = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.fulldelaymark")) {
            pThis->iFullDlyMrk = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.lightdelaymark")) {
            pThis->iLightDlyMrk = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.discardmark")) {
            pThis->iDiscardMrk = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.discardseverity")) {
            pThis->iDiscardSeverity = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.checkpointinterval")) {
            pThis->iPersistUpdCnt = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.syncqueuefiles")) {
            pThis->bSyncQueueFiles = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.type")) {
            pThis->qType = (queueType_t)pvals[i].val.d.n;
            if (pThis->qType == QUEUETYPE_DIRECT) {
                /* if we have a direct queue, we mimic this param was not set.
                 * Our prime intent is to make sure we detect when "real" params
                 * are set on a direct queue, and the type setting is obviously
                 * not relevant here.
                 */
                n_params_set--;
            }
        } else if (!strcmp(pblk.descr[i].name, "queue.diskqueuetype")) {
            char *mode;
            CHKmalloc(mode = es_str2cstr(pvals[i].val.d.estr, NULL));
            pThis->diskQueueTypeSet = 1;
            if (!strcasecmp(mode, "auto"))
                pThis->diskQueueType = QDA_ENGINE_AUTO;
            else if (!strcasecmp(mode, "disk"))
                pThis->diskQueueType = QDA_ENGINE_DISK;
            else if (!strcasecmp(mode, "segmentedDisk"))
                pThis->diskQueueType = QDA_ENGINE_SEGMENTED_DISK;
            else {
                parser_errmsg("queue.diskQueueType: invalid value '%s'; using 'auto'", mode);
                pThis->diskQueueType = QDA_ENGINE_AUTO;
            }
            free(mode);
        } else if (!strcmp(pblk.descr[i].name, "queue.diskqueueautoupgrade")) {
            pThis->diskQueueAutoUpgrade = pvals[i].val.d.n;
            pThis->diskQueueAutoUpgradeSet = 1;
        } else if (!strcmp(pblk.descr[i].name, "queue.diskqueueidletimeout")) {
            pThis->diskQueueIdleTimeout = pvals[i].val.d.n;
            pThis->diskQueueIdleTimeoutSet = 1;
            if (pThis->diskQueueIdleTimeout < -1) {
                parser_errmsg("queue.diskQueueIdleTimeout must be -1 or greater; using 60000");
                pThis->diskQueueIdleTimeout = 60000;
            }
        } else if (!strcmp(pblk.descr[i].name, "queue.workerthreads")) {
            CHKiRet(qqueueSetiNumWorkerThreads(pThis, pvals[i].val.d.n));
        } else if (!strcmp(pblk.descr[i].name, "queue.timeoutshutdown")) {
            pThis->toQShutdown = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.timeoutactioncompletion")) {
            pThis->toActShutdown = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.timeoutenqueue")) {
            pThis->toEnq = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.timeoutworkerthreadshutdown")) {
            pThis->toWrkShutdown = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.workerthreadminimummessages")) {
            pThis->iMinMsgsPerWrkr = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.maxfilesize")) {
            pThis->iMaxFileSize = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.saveonshutdown")) {
            pThis->bSaveOnShutdown = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.dequeueslowdown")) {
            pThis->iDeqSlowdown = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.dequeuetimebegin")) {
            pThis->iDeqtWinFromHr = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.dequeuetimeend")) {
            pThis->iDeqtWinToHr = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.samplinginterval")) {
            pThis->iSmpInterval = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.takeflowctlfrommsg")) {
            pThis->takeFlowCtlFromMsg = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.mutexcontentionstats")) {
            pThis->bMutexContentionStats = pvals[i].val.d.n;
        } else if (!strcmp(pblk.descr[i].name, "queue.oncorruption")) {
            char *mode;
            CHKmalloc(mode = es_str2cstr(pvals[i].val.d.estr, NULL));
            if (!strcasecmp(mode, "ignore")) {
                pThis->onCorruption = QUEUE_ON_CORRUPTION_IGNORE;
            } else if (!strcasecmp(mode, "safe")) {
                pThis->onCorruption = QUEUE_ON_CORRUPTION_SAFE_MODE;
            } else if (!strcasecmp(mode, "inMemory")) {
                pThis->onCorruption = QUEUE_ON_CORRUPTION_IN_MEMORY;
            } else {
                LogError(0, RS_RET_CONF_PARAM_INVLD, "queue.oncorruption: invalid value '%s', using 'safe'", mode);
                pThis->onCorruption = QUEUE_ON_CORRUPTION_SAFE_MODE;
            }
            free(mode);
        } else {
            DBGPRINTF(
                "queue: program error, non-handled "
                "param '%s'\n",
                pblk.descr[i].name);
        }
    }

    CHKiRet(qqueueValidateLocalConfig(pThis));

    const sbool is_da_memory_queue =
        (pThis->qType == QUEUETYPE_FIXED_ARRAY || pThis->qType == QUEUETYPE_LINKEDLIST) && pThis->pszFilePrefix != NULL;
    /* These are deliberately parser_errmsg(), not advisory warnings:
     * parser_errmsg marks the configuration dirty.  Permissive startup keeps
     * running after the unusable value is ignored, while
     * abortOnUncleanConfig="on" rejects the same configuration. */
    if ((pThis->diskQueueTypeSet || pThis->diskQueueAutoUpgradeSet || pThis->diskQueueIdleTimeoutSet) &&
        !is_da_memory_queue) {
        parser_errmsg(
            "queue.diskQueueType, queue.diskQueueAutoUpgrade, and queue.diskQueueIdleTimeout apply only to "
            "FixedArray or LinkedList disk-assisted queues; ignoring these parameters");
        pThis->diskQueueType = QDA_ENGINE_AUTO;
        pThis->diskQueueAutoUpgrade = 0;
        pThis->diskQueueIdleTimeout = 60000;
    }
    if (is_da_memory_queue && pThis->diskQueueAutoUpgradeSet && pThis->diskQueueType != QDA_ENGINE_AUTO) {
        parser_errmsg("queue.diskQueueAutoUpgrade applies only when queue.diskQueueType='auto'; ignoring it");
        pThis->diskQueueAutoUpgrade = 0;
    }
    if (is_da_memory_queue && pThis->diskQueueType == QDA_ENGINE_DISK && pThis->diskQueueIdleTimeoutSet &&
        pThis->diskQueueIdleTimeout != 60000) {
        parser_errmsg("queue.diskQueueIdleTimeout does not apply to the classic disk engine; using 60000");
        pThis->diskQueueIdleTimeout = 60000;
    }

    checkUniqueDiskFile(pThis);

    if (pThis->qType == QUEUETYPE_DIRECT) {
        if (n_params_set > 0) {
            LogMsg(0, RS_RET_OK, LOG_WARNING,
                   "warning on queue '%s': "
                   "queue is in direct mode, but parameters have been set. "
                   "These PARAMETERS cannot be applied and WILL BE IGNORED.",
                   obj.GetName((obj_t *)pThis));
        }
    } else if (pThis->qType == QUEUETYPE_DISK) {
        if (pThis->pszFilePrefix == NULL) {
            LogError(0, RS_RET_QUEUE_DISK_NO_FN,
                     "error on queue '%s', disk mode selected, but "
                     "no queue file name given; queue type changed to 'linkedList'",
                     obj.GetName((obj_t *)pThis));
            pThis->qType = QUEUETYPE_LINKEDLIST;
        }
    } else if (pThis->qType == QUEUETYPE_SEGMENTED_DISK) {
        if (pThis->pszFilePrefix == NULL) {
            LogError(0, RS_RET_QUEUE_DISK_NO_FN,
                     "error on queue '%s', segmentedDisk mode selected, but "
                     "no queue file name given",
                     obj.GetName((obj_t *)pThis));
            ABORT_FINALIZE(RS_RET_QUEUE_DISK_NO_FN);
        }
        if (pThis->onCorruption != QUEUE_ON_CORRUPTION_SAFE_MODE) {
            LogError(0, RS_RET_CONF_PARAM_INVLD,
                     "error on queue '%s': segmentedDisk currently supports only queue.onCorruption=\"safe\"",
                     obj.GetName((obj_t *)pThis));
            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
        }
        if (pThis->cryprovName != NULL) {
            LogError(0, RS_RET_CONF_PARAM_INVLD,
                     "error on queue '%s': queue.cry.provider is not yet supported by segmentedDisk",
                     obj.GetName((obj_t *)pThis));
            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
        }
    }

    if (pThis->pszFilePrefix == NULL && pThis->cryprovName != NULL) {
        LogError(0, RS_RET_QUEUE_CRY_DISK_ONLY,
                 "error on queue '%s', crypto provider can "
                 "only be set for disk or disk assisted queue - ignored",
                 obj.GetName((obj_t *)pThis));
        free(pThis->cryprovName);
        pThis->cryprovName = NULL;
    }

    if (pThis->cryprovName != NULL) {
        CHKiRet(initCryprov(pThis, lst));
    }

finalize_it:
    cnfparamvalsDestruct(pvals, &pblk);
    if (iRet != RS_RET_OK && loadConf->bLocalConfigRequested) {
        pThis->bLocalConfigError = 1;
        loadConf->bLocalConfigError = 1;
        iRet = RS_RET_LOCAL_QUEUE_CONFIG;
    }
    RETiRet;
}

/* return 1 if the content of two qqueue_t structs equal */
int queuesEqual(qqueue_t *pOld, qqueue_t *pNew) {
    const qda_lifecycle_config_t old_da = {
        .engine = pOld->diskQueueType,
        .auto_upgrade = pOld->diskQueueAutoUpgrade,
        .idle_timeout = pOld->diskQueueIdleTimeout,
    };
    const qda_lifecycle_config_t new_da = {
        .engine = pNew->diskQueueType,
        .auto_upgrade = pNew->diskQueueAutoUpgrade,
        .idle_timeout = pNew->diskQueueIdleTimeout,
    };
    return (NUM_EQUALS(qType) && NUM_EQUALS(iMaxQueueSize) && NUM_EQUALS(iDeqBatchSize) &&
            NUM_EQUALS(iMinDeqBatchSize) && NUM_EQUALS(toMinDeqBatchSize) && NUM_EQUALS(sizeOnDiskMax) &&
            NUM_EQUALS(iHighWtrMrk) && NUM_EQUALS(iLowWtrMrk) && NUM_EQUALS(iFullDlyMrk) && NUM_EQUALS(iLightDlyMrk) &&
            NUM_EQUALS(iDiscardMrk) && NUM_EQUALS(iDiscardSeverity) && NUM_EQUALS(iPersistUpdCnt) &&
            NUM_EQUALS(bSyncQueueFiles) && NUM_EQUALS(iNumWorkerThreads) && NUM_EQUALS(toQShutdown) &&
            NUM_EQUALS(toActShutdown) && NUM_EQUALS(toEnq) && NUM_EQUALS(toWrkShutdown) &&
            NUM_EQUALS(iMinMsgsPerWrkr) && NUM_EQUALS(iMaxFileSize) && NUM_EQUALS(bSaveOnShutdown) &&
            NUM_EQUALS(iDeqSlowdown) && NUM_EQUALS(iDeqtWinFromHr) && NUM_EQUALS(iDeqtWinToHr) &&
            NUM_EQUALS(iSmpInterval) && NUM_EQUALS(bMutexContentionStats) && NUM_EQUALS(takeFlowCtlFromMsg) &&
            qdaLifecycleConfigEqual(&old_da, &new_da) && USTR_EQUALS(pszFilePrefix) && USTR_EQUALS(cryprovName));
}


/* some simple object access methods
 * Note: the semicolons behind the macros are actually empty declarations. This is
 * a work-around for clang-format's missing understanding of generative macros.
 * Some compilers may flag this empty declarations by a warning. If so, we need
 * to disable this warning. Alternatively, we could exclude this code from being
 * reformatted by clang-format;
 */
DEFpropSetMeth(qqueue, bSyncQueueFiles, int);
DEFpropSetMeth(qqueue, iPersistUpdCnt, int);
DEFpropSetMeth(qqueue, iDeqtWinFromHr, int);
DEFpropSetMeth(qqueue, iDeqtWinToHr, int);
DEFpropSetMeth(qqueue, toQShutdown, long);
DEFpropSetMeth(qqueue, toActShutdown, long);
DEFpropSetMeth(qqueue, toWrkShutdown, long);
DEFpropSetMeth(qqueue, toEnq, long);
DEFpropSetMeth(qqueue, iHighWtrMrk, int);
DEFpropSetMeth(qqueue, iLowWtrMrk, int);
DEFpropSetMeth(qqueue, iDiscardMrk, int);
DEFpropSetMeth(qqueue, iDiscardSeverity, int);
DEFpropSetMeth(qqueue, iLightDlyMrk, int);
DEFpropSetMeth(qqueue, iMinMsgsPerWrkr, int);
DEFpropSetMeth(qqueue, bSaveOnShutdown, int);
DEFpropSetMeth(qqueue, pAction, action_t *);
DEFpropSetMeth(qqueue, iDeqSlowdown, int);
DEFpropSetMeth(qqueue, iDeqBatchSize, int);
DEFpropSetMeth(qqueue, iMinDeqBatchSize, int);
DEFpropSetMeth(qqueue, sizeOnDiskMax, int64);
DEFpropSetMeth(qqueue, iSmpInterval, int);

rsRetVal qqueueSetiNumWorkerThreads(qqueue_t *pThis, int pVal) {
    if (pVal <= 0) {
        LogError(0, RS_RET_CONF_PARAM_INVLD, "queue.workerthreads must be greater than 0, but is %d", pVal);
        return RS_RET_CONF_PARAM_INVLD;
    }

    pThis->iNumWorkerThreads = pVal;
    return RS_RET_OK;
}

/* This function can be used as a generic way to set properties. Only the subset
 * of properties required to read persisted property bags is supported. This
 * functions shall only be called by the property bag reader, thus it is static.
 * rgerhards, 2008-01-11
 */
#define isProp(name) !rsCStrSzStrCmp(pProp->pcsName, (uchar *)name, sizeof(name) - 1)
static rsRetVal qqueueSetProperty(qqueue_t *pThis, var_t *pProp) {
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, qqueue);
    assert(pProp != NULL);

    if (isProp("iQueueSize")) {
        pThis->iQueueSize = pProp->val.num;
#ifdef ENABLE_IMDIAG
        iOverallQueueSize += pThis->iQueueSize;
#endif
    } else if (isProp("tVars.disk.sizeOnDisk")) {
        pThis->tVars.disk.sizeOnDisk = pProp->val.num;
    } else if (isProp("qType")) {
        if (pThis->qType != pProp->val.num) ABORT_FINALIZE(RS_RET_QTYPE_MISMATCH);
    }

finalize_it:
    RETiRet;
}
#undef isProp

/* dummy */
static rsRetVal qqueueQueryInterface(interface_t __attribute__((unused)) * i) {
    return RS_RET_NOT_IMPLEMENTED;
}

/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(qqueue, 1, OBJ_IS_CORE_MODULE)
    /* request objects we use */
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(strm, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
    CHKiRet(qqueueLocalStatsClassInit());

    /* now set our own handlers */
    OBJSetMethodHandler(objMethod_SETPROPERTY, qqueueSetProperty);
ENDObjClassInit(qqueue)
