/* Snapshot-backed impstats for experimental local queue frontends.
 *
 * Concurrency & Locking:
 * - A pre-read callback runs with the stats-object-list lock held.  It calls
 *   only the lock-free qqueueLocal*Snapshot accessors and relaxed atomic
 *   stores to this adapter's counter storage.
 * - No enqueue, dequeue, completion, registration, or shutdown path takes a
 *   stats lock or calls this adapter.
 * - Destruction unlinks all objects before the local family is released.  The
 *   stats list lock serializes that unlink with a running pre-read callback.
 */
#include "config.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "rsyslog.h"
#include "queue.h"
#include "queue_local.h"
#include "queue_local_stats.h"
#include "statsobj.h"
#include "unicode-helper.h"

DEFobjStaticHelpers;
DEFobjCurrIf(statsobj);

enum localLogicalCounter {
    localLogicalSampledOut,
    localLogicalSeverityDiscarded,
    localLogicalRetryFailed,
    localLogicalReserved,
    localLogicalBeCapacity,
    localLogicalFeCapacity,
    localLogicalFeActiveCapacity,

    localLogicalIngressMessages,
    localLogicalAcceptedMessages,
    localLogicalRejectedPreadmissionMessages,
    localLogicalTerminalMessages,
    localLogicalRestored,
    localLogicalPersisted,
    localLogicalExecuted,
    localLogicalDiscarded,
    localLogicalDiskTransferred,
    localLogicalDiskTerminal,
    localLogicalDiskPhysical,
    localLogicalDiskActive,

    localLogicalTerminalShutdownDiscardedMessages,
    localLogicalOutstandingMessages,
    localLogicalRouteFeMessages,
    localLogicalRouteFeBatches,
    localLogicalRouteBeMessages,
    localLogicalRouteBeBatches,
    localLogicalRouteBeAcceptedMessages,
    localLogicalBeTerminalMessages,
    localLogicalRouteBeNoFitMessages,
    localLogicalRouteBeOversizedMessages,
    localLogicalRouteBeRegistrationFallbackMessages,
    localLogicalRouteBeShutdownRedirectMessages,
    localLogicalRouteBeUnclassifiedMessages,
    localLogicalRouteBeInternalMessages,
    localLogicalRouteBeCapacityExhaustedMessages,
    localLogicalBePhysicalMessages,
    localLogicalBeActiveMessages,
    localLogicalFeQueuedMessages,
    localLogicalFeActiveMessages,
    localLogicalFeRetryMessages,
    localLogicalFeRegistered,
    localLogicalFeStarted,
    localLogicalFeProducerless,
    localLogicalFeRegistrationFailures,
    localLogicalWorkersFe,
    localLogicalWorkersBe,
    localLogicalConfiguredFrontends,
    localLogicalTransferFeToBeMessages,
    localLogicalAllocationBytes,
    localLogicalBeDequeueBatches,
    localLogicalBeDequeueMessages,
    localLogicalBeDequeueMax,
    localLogicalHelpAttempts,
    localLogicalHelpEmpty,
    localLogicalHelpBatches,
    localLogicalHelpMessages,
    localLogicalHelpMax,
    localLogicalHelpActive,
    localLogicalHelpRetry,
    localLogicalHelpTerminal,
    localLogicalHelpWaits,
    localLogicalHelpWakes,
    localLogicalHelpLimit,
    localLogicalHelpCompleted,
    localLogicalHelpReturned,
    localLogicalHelpWakeFe,
    localLogicalHelpWakeShutdown,
    localLogicalCounterCount
};

enum localFrontendCounter {
    localFrontendRegistrationId,
    localFrontendGeneration,
    localFrontendLifecycleState,
    localFrontendCapacity,
    localFrontendQueued,
    localFrontendFree,
    localFrontendInflight,
    localFrontendRetry,
    localFrontendIngressMessages,
    localFrontendAdmittedMessages,
    localFrontendAdmittedBatches,
    localFrontendSubmittedBatches,
    localFrontendSubmittedMessages,
    localFrontendSubmittedMax,
    localFrontendTerminalMessages,
    localFrontendOverflowMessages,
    localFrontendOverflowNoFitMessages,
    localFrontendOverflowOversizedMessages,
    localFrontendOverflowBatches,
    localFrontendOversizedBatches,
    localFrontendTransferToBeMessages,
    localFrontendShutdownDiscardedMessages,
    localFrontendProducerExited,
    localFrontendDequeueBatches,
    localFrontendDequeueMessages,
    localFrontendDequeueMax,
    localFrontendBytes,
    localFrontendHelpAttempts,
    localFrontendHelpEmpty,
    localFrontendHelpBatches,
    localFrontendHelpMessages,
    localFrontendHelpMax,
    localFrontendHelpActive,
    localFrontendHelpRetry,
    localFrontendHelpTerminal,
    localFrontendHelpWaits,
    localFrontendHelpWakes,
    localFrontendHelpLimit,
    localFrontendHelpCompleted,
    localFrontendHelpReturned,
    localFrontendHelpWakeFe,
    localFrontendHelpWakeShutdown,
    localFrontendCounterCount
};

typedef struct localFrontendStats_s {
    struct qqueueLocalStats_s *adapter;
    uint32_t index;
    statsobj_t *object;
    intctr_t counters[localFrontendCounterCount];
} localFrontendStats_t;

struct qqueueLocalStats_s {
    qqueue_t *owner;
    statsobj_t *logical;
    intctr_t logical_counters[localLogicalCounterCount];
    uint32_t frontend_limit;
    localFrontendStats_t *frontends;
#ifdef ENABLE_TESTBENCH
    qqueueLocalStatsTestPreReadHook_t test_pre_read_hook;
    void *test_pre_read_context;
#endif
};

typedef struct localCounterDescriptor_s {
    const char *name;
    size_t index;
} localCounterDescriptor_t;

rsRetVal qqueueLocalStatsClassInit(void) {
    DEFiRet;
    CHKiRet(objGetObjInterface(&obj));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
finalize_it:
    RETiRet;
}

#define LOCAL_COUNTER(name, member) \
    { name, member }
static const localCounterDescriptor_t logicalCounters[] = {
    LOCAL_COUNTER("policy.sampled_out.messages", localLogicalSampledOut),
    LOCAL_COUNTER("policy.severity_discarded.messages", localLogicalSeverityDiscarded),
    LOCAL_COUNTER("retry.enqueue_failed.messages", localLogicalRetryFailed),
    LOCAL_COUNTER("resource.reserved.messages", localLogicalReserved),
    LOCAL_COUNTER("resource.be.capacity.messages", localLogicalBeCapacity),
    LOCAL_COUNTER("resource.fe.capacity.messages", localLogicalFeCapacity),
    LOCAL_COUNTER("resource.fe.active.capacity.messages", localLogicalFeActiveCapacity),

    LOCAL_COUNTER("ingress.messages", localLogicalIngressMessages),
    LOCAL_COUNTER("accepted.messages", localLogicalAcceptedMessages),
    LOCAL_COUNTER("rejected.preadmission.messages", localLogicalRejectedPreadmissionMessages),
    LOCAL_COUNTER("terminal.messages", localLogicalTerminalMessages),
    LOCAL_COUNTER("restored.messages", localLogicalRestored),
    LOCAL_COUNTER("terminal.persisted.messages", localLogicalPersisted),
    LOCAL_COUNTER("terminal.executed.messages", localLogicalExecuted),
    LOCAL_COUNTER("terminal.discarded.messages", localLogicalDiscarded),
    LOCAL_COUNTER("transfer.be_to_disk.messages", localLogicalDiskTransferred),
    LOCAL_COUNTER("disk.terminal.messages", localLogicalDiskTerminal),
    LOCAL_COUNTER("disk.physical.messages", localLogicalDiskPhysical),
    LOCAL_COUNTER("disk.active.messages", localLogicalDiskActive),

    LOCAL_COUNTER("terminal.shutdown_discarded.messages", localLogicalTerminalShutdownDiscardedMessages),
    LOCAL_COUNTER("outstanding.messages", localLogicalOutstandingMessages),
    LOCAL_COUNTER("route.fe.messages", localLogicalRouteFeMessages),
    LOCAL_COUNTER("route.fe.batches", localLogicalRouteFeBatches),
    LOCAL_COUNTER("route.be.messages", localLogicalRouteBeMessages),
    LOCAL_COUNTER("route.be.batches", localLogicalRouteBeBatches),
    LOCAL_COUNTER("route.be.accepted.messages", localLogicalRouteBeAcceptedMessages),
    /* Physical backend terminal completions include FE-to-BE transfers. */
    LOCAL_COUNTER("be.terminal.messages", localLogicalBeTerminalMessages),
    LOCAL_COUNTER("route.be.reason.nofit.messages", localLogicalRouteBeNoFitMessages),
    LOCAL_COUNTER("route.be.reason.oversized.messages", localLogicalRouteBeOversizedMessages),
    LOCAL_COUNTER("route.be.reason.registration_fallback.messages", localLogicalRouteBeRegistrationFallbackMessages),
    LOCAL_COUNTER("route.be.reason.shutdown_redirect.messages", localLogicalRouteBeShutdownRedirectMessages),
    LOCAL_COUNTER("route.be.reason.unclassified.messages", localLogicalRouteBeUnclassifiedMessages),
    LOCAL_COUNTER("route.be.reason.internal.messages", localLogicalRouteBeInternalMessages),
    LOCAL_COUNTER("route.be.reason.capacity_exhausted.messages", localLogicalRouteBeCapacityExhaustedMessages),
    LOCAL_COUNTER("be.physical.messages", localLogicalBePhysicalMessages),
    LOCAL_COUNTER("be.active.messages", localLogicalBeActiveMessages),
    LOCAL_COUNTER("fe.queued.messages", localLogicalFeQueuedMessages),
    LOCAL_COUNTER("fe.inflight.messages", localLogicalFeActiveMessages),
    LOCAL_COUNTER("fe.retry.messages", localLogicalFeRetryMessages),
    LOCAL_COUNTER("fe.registered", localLogicalFeRegistered),
    LOCAL_COUNTER("fe.started", localLogicalFeStarted),
    LOCAL_COUNTER("fe.producerless", localLogicalFeProducerless),
    LOCAL_COUNTER("fe.registration_failures", localLogicalFeRegistrationFailures),
    LOCAL_COUNTER("workers.fe", localLogicalWorkersFe),
    LOCAL_COUNTER("workers.be", localLogicalWorkersBe),
    LOCAL_COUNTER("capacity.frontends", localLogicalConfiguredFrontends),
    LOCAL_COUNTER("transfer.fe_to_be.messages", localLogicalTransferFeToBeMessages),
    LOCAL_COUNTER("allocation.bytes", localLogicalAllocationBytes),
    LOCAL_COUNTER("batch.be_dequeue.count", localLogicalBeDequeueBatches),
    LOCAL_COUNTER("batch.be_dequeue.messages.sum", localLogicalBeDequeueMessages),
    LOCAL_COUNTER("batch.be_dequeue.messages.max", localLogicalBeDequeueMax),
    LOCAL_COUNTER("help.attempts", localLogicalHelpAttempts),
    LOCAL_COUNTER("help.empty", localLogicalHelpEmpty),
    LOCAL_COUNTER("batch.help.count", localLogicalHelpBatches),
    LOCAL_COUNTER("batch.help.messages.sum", localLogicalHelpMessages),
    LOCAL_COUNTER("batch.help.messages.max", localLogicalHelpMax),
    LOCAL_COUNTER("inflight.help", localLogicalHelpActive),
    LOCAL_COUNTER("retry.help", localLogicalHelpRetry),
    LOCAL_COUNTER("terminal.help", localLogicalHelpTerminal),
    LOCAL_COUNTER("help.waits", localLogicalHelpWaits),
    LOCAL_COUNTER("help.wakes", localLogicalHelpWakes),
    LOCAL_COUNTER("batch.help.limit", localLogicalHelpLimit),
    LOCAL_COUNTER("help.completed", localLogicalHelpCompleted),
    LOCAL_COUNTER("help.returned", localLogicalHelpReturned),
    LOCAL_COUNTER("wake.fe", localLogicalHelpWakeFe),
    LOCAL_COUNTER("wake.shutdown", localLogicalHelpWakeShutdown),
};
#undef LOCAL_COUNTER

#define FRONTEND_COUNTER(name, member) \
    { name, member }
static const localCounterDescriptor_t frontendCounters[] = {
    FRONTEND_COUNTER("registration.id", localFrontendRegistrationId),
    FRONTEND_COUNTER("registration.generation", localFrontendGeneration),
    FRONTEND_COUNTER("lifecycle.state", localFrontendLifecycleState),
    FRONTEND_COUNTER("capacity", localFrontendCapacity),
    FRONTEND_COUNTER("queued", localFrontendQueued),
    FRONTEND_COUNTER("free", localFrontendFree),
    FRONTEND_COUNTER("inflight.fe", localFrontendInflight),
    FRONTEND_COUNTER("retry.fe", localFrontendRetry),
    FRONTEND_COUNTER("ingress.messages", localFrontendIngressMessages),
    FRONTEND_COUNTER("admitted.messages", localFrontendAdmittedMessages),
    FRONTEND_COUNTER("admitted.batches", localFrontendAdmittedBatches),
    FRONTEND_COUNTER("batch.submit.count", localFrontendSubmittedBatches),
    FRONTEND_COUNTER("batch.submit.messages.sum", localFrontendSubmittedMessages),
    FRONTEND_COUNTER("batch.submit.messages.max", localFrontendSubmittedMax),
    FRONTEND_COUNTER("terminal.messages", localFrontendTerminalMessages),
    FRONTEND_COUNTER("overflow.messages", localFrontendOverflowMessages),
    FRONTEND_COUNTER("overflow.nofit.messages", localFrontendOverflowNoFitMessages),
    FRONTEND_COUNTER("overflow.oversized.messages", localFrontendOverflowOversizedMessages),
    FRONTEND_COUNTER("overflow.batches", localFrontendOverflowBatches),
    FRONTEND_COUNTER("overflow.oversized_batches", localFrontendOversizedBatches),
    FRONTEND_COUNTER("transfer.fe_to_be.messages", localFrontendTransferToBeMessages),
    FRONTEND_COUNTER("shutdown.discarded.messages", localFrontendShutdownDiscardedMessages),
    FRONTEND_COUNTER("producer.exited", localFrontendProducerExited),
    FRONTEND_COUNTER("batch.fe_dequeue.count", localFrontendDequeueBatches),
    FRONTEND_COUNTER("batch.fe_dequeue.messages.sum", localFrontendDequeueMessages),
    FRONTEND_COUNTER("batch.fe_dequeue.messages.max", localFrontendDequeueMax),
    FRONTEND_COUNTER("admitted.raw_bytes", localFrontendBytes),
    FRONTEND_COUNTER("help.attempts", localFrontendHelpAttempts),
    FRONTEND_COUNTER("help.empty", localFrontendHelpEmpty),
    FRONTEND_COUNTER("batch.help.count", localFrontendHelpBatches),
    FRONTEND_COUNTER("batch.help.messages.sum", localFrontendHelpMessages),
    FRONTEND_COUNTER("batch.help.messages.max", localFrontendHelpMax),
    FRONTEND_COUNTER("inflight.help", localFrontendHelpActive),
    FRONTEND_COUNTER("retry.help", localFrontendHelpRetry),
    FRONTEND_COUNTER("terminal.help", localFrontendHelpTerminal),
    FRONTEND_COUNTER("help.waits", localFrontendHelpWaits),
    FRONTEND_COUNTER("help.wakes", localFrontendHelpWakes),
    FRONTEND_COUNTER("batch.help.limit", localFrontendHelpLimit),
    FRONTEND_COUNTER("help.completed", localFrontendHelpCompleted),
    FRONTEND_COUNTER("help.returned", localFrontendHelpReturned),
    FRONTEND_COUNTER("wake.fe", localFrontendHelpWakeFe),
    FRONTEND_COUNTER("wake.shutdown", localFrontendHelpWakeShutdown),
};
#undef FRONTEND_COUNTER

static void localStatsStore(intctr_t *const counter, const uint64_t value) {
    PREFER_STORE_uint64(counter, value);
}

static void localStatsRunTestPreReadHook(qqueueLocalStats_t *const stats) {
#ifdef ENABLE_TESTBENCH
    if (stats->test_pre_read_hook != NULL) stats->test_pre_read_hook(stats->test_pre_read_context);
#else
    (void)stats;
#endif
}

static void localStatsPreRead(statsobj_t *const object, void *const context) {
    (void)object;
    qqueueLocalStats_t *const stats = context;
    qqueueLocalSnapshot_t snapshot;

    localStatsRunTestPreReadHook(stats);
    qqueueLocalGetSnapshot(stats->owner, &snapshot);
    localStatsStore(&stats->logical_counters[localLogicalSampledOut], snapshot.sampled_out);
    localStatsStore(&stats->logical_counters[localLogicalSeverityDiscarded], snapshot.severity_discarded);
    localStatsStore(&stats->logical_counters[localLogicalRetryFailed], snapshot.retry_failed);
    localStatsStore(&stats->logical_counters[localLogicalReserved], snapshot.reserved_messages);
    localStatsStore(&stats->logical_counters[localLogicalBeCapacity], snapshot.be_capacity);
    localStatsStore(&stats->logical_counters[localLogicalFeCapacity], snapshot.fe_capacity);
    localStatsStore(&stats->logical_counters[localLogicalFeActiveCapacity], snapshot.fe_active_capacity);

    localStatsStore(&stats->logical_counters[localLogicalIngressMessages], snapshot.attempts);
    localStatsStore(&stats->logical_counters[localLogicalAcceptedMessages], snapshot.admitted);
    localStatsStore(&stats->logical_counters[localLogicalRejectedPreadmissionMessages], snapshot.preadmission_rejected);
    localStatsStore(&stats->logical_counters[localLogicalTerminalMessages], snapshot.terminal);
    localStatsStore(&stats->logical_counters[localLogicalRestored], snapshot.restored);
    localStatsStore(&stats->logical_counters[localLogicalPersisted], snapshot.persisted);
    localStatsStore(&stats->logical_counters[localLogicalExecuted], snapshot.executed);
    localStatsStore(&stats->logical_counters[localLogicalDiscarded], snapshot.discarded);
    localStatsStore(&stats->logical_counters[localLogicalDiskTransferred], snapshot.disk_transferred);
    localStatsStore(&stats->logical_counters[localLogicalDiskTerminal], snapshot.disk_terminal);
    localStatsStore(&stats->logical_counters[localLogicalDiskPhysical], snapshot.disk_physical);
    localStatsStore(&stats->logical_counters[localLogicalDiskActive], snapshot.disk_active);

    localStatsStore(&stats->logical_counters[localLogicalTerminalShutdownDiscardedMessages],
                    snapshot.shutdown_discarded);
    localStatsStore(&stats->logical_counters[localLogicalOutstandingMessages], snapshot.outstanding);
    localStatsStore(&stats->logical_counters[localLogicalRouteFeMessages], snapshot.route_fe_messages);
    localStatsStore(&stats->logical_counters[localLogicalRouteFeBatches], snapshot.route_fe_batches);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeMessages], snapshot.route_be_messages);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeBatches], snapshot.route_be_batches);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeAcceptedMessages], snapshot.be_admitted);
    localStatsStore(&stats->logical_counters[localLogicalBeTerminalMessages], snapshot.be_terminal);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeNoFitMessages], snapshot.be_nofit);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeOversizedMessages], snapshot.be_oversized);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeRegistrationFallbackMessages],
                    snapshot.be_registration_fallback);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeShutdownRedirectMessages],
                    snapshot.be_shutdown_redirect);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeUnclassifiedMessages], snapshot.be_unclassified);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeInternalMessages], snapshot.be_internal);
    localStatsStore(&stats->logical_counters[localLogicalRouteBeCapacityExhaustedMessages],
                    snapshot.be_capacity_exhausted);
    localStatsStore(&stats->logical_counters[localLogicalBePhysicalMessages], snapshot.be_physical);
    localStatsStore(&stats->logical_counters[localLogicalBeActiveMessages], snapshot.be_active);
    localStatsStore(&stats->logical_counters[localLogicalFeQueuedMessages], snapshot.fe_queued);
    localStatsStore(&stats->logical_counters[localLogicalFeActiveMessages], snapshot.fe_active);
    localStatsStore(&stats->logical_counters[localLogicalFeRetryMessages], snapshot.fe_retry);
    localStatsStore(&stats->logical_counters[localLogicalFeRegistered], snapshot.fe_registered);
    localStatsStore(&stats->logical_counters[localLogicalFeStarted], snapshot.fe_started);
    localStatsStore(&stats->logical_counters[localLogicalFeProducerless], snapshot.fe_producerless);
    localStatsStore(&stats->logical_counters[localLogicalFeRegistrationFailures], snapshot.registration_failures);
    localStatsStore(&stats->logical_counters[localLogicalWorkersFe], snapshot.fe_consumers);
    localStatsStore(&stats->logical_counters[localLogicalWorkersBe], snapshot.be_consumers);
    localStatsStore(&stats->logical_counters[localLogicalConfiguredFrontends], snapshot.configured_frontends);
    localStatsStore(&stats->logical_counters[localLogicalTransferFeToBeMessages], snapshot.transferred);
    localStatsStore(&stats->logical_counters[localLogicalAllocationBytes], snapshot.allocation_bytes);
    localStatsStore(&stats->logical_counters[localLogicalBeDequeueBatches], snapshot.be_dequeue_batches);
    localStatsStore(&stats->logical_counters[localLogicalBeDequeueMessages], snapshot.be_dequeue_messages);
    localStatsStore(&stats->logical_counters[localLogicalBeDequeueMax], snapshot.be_dequeue_max);
    localStatsStore(&stats->logical_counters[localLogicalHelpAttempts], snapshot.help_attempts);
    localStatsStore(&stats->logical_counters[localLogicalHelpEmpty], snapshot.help_empty);
    localStatsStore(&stats->logical_counters[localLogicalHelpBatches], snapshot.help_batches);
    localStatsStore(&stats->logical_counters[localLogicalHelpMessages], snapshot.help_messages);
    localStatsStore(&stats->logical_counters[localLogicalHelpMax], snapshot.help_max);
    localStatsStore(&stats->logical_counters[localLogicalHelpActive], snapshot.help_active);
    localStatsStore(&stats->logical_counters[localLogicalHelpRetry], snapshot.help_retry);
    localStatsStore(&stats->logical_counters[localLogicalHelpTerminal], snapshot.help_terminal);
    localStatsStore(&stats->logical_counters[localLogicalHelpWaits], snapshot.help_waits);
    localStatsStore(&stats->logical_counters[localLogicalHelpWakes], snapshot.help_wakes);
    localStatsStore(&stats->logical_counters[localLogicalHelpLimit], snapshot.help_limit);
    localStatsStore(&stats->logical_counters[localLogicalHelpCompleted], snapshot.help_completed);
    localStatsStore(&stats->logical_counters[localLogicalHelpReturned], snapshot.help_returned);
    localStatsStore(&stats->logical_counters[localLogicalHelpWakeFe], snapshot.wake_fe);
    localStatsStore(&stats->logical_counters[localLogicalHelpWakeShutdown], snapshot.wake_shutdown);
}

static void localFrontendStatsPreRead(statsobj_t *const object, void *const context) {
    (void)object;
    localFrontendStats_t *const frontend = context;
    qqueueLocalFrontendSnapshot_t snapshot;

    localStatsRunTestPreReadHook(frontend->adapter);
    if (!qqueueLocalGetFrontendSnapshot(frontend->adapter->owner, frontend->index, &snapshot)) return;
    localStatsStore(&frontend->counters[localFrontendRegistrationId], snapshot.identity);
    localStatsStore(&frontend->counters[localFrontendGeneration], snapshot.generation);
    localStatsStore(&frontend->counters[localFrontendLifecycleState], snapshot.state);
    localStatsStore(&frontend->counters[localFrontendCapacity], snapshot.capacity);
    localStatsStore(&frontend->counters[localFrontendQueued], snapshot.queued);
    localStatsStore(&frontend->counters[localFrontendFree],
                    snapshot.capacity >= snapshot.queued ? snapshot.capacity - snapshot.queued : 0);
    localStatsStore(&frontend->counters[localFrontendInflight], snapshot.active);
    localStatsStore(&frontend->counters[localFrontendRetry], snapshot.retry);
    localStatsStore(&frontend->counters[localFrontendIngressMessages], snapshot.attempts);
    localStatsStore(&frontend->counters[localFrontendAdmittedMessages], snapshot.published);
    localStatsStore(&frontend->counters[localFrontendAdmittedBatches], snapshot.published_batches);
    localStatsStore(&frontend->counters[localFrontendSubmittedBatches], snapshot.submitted_batches);
    localStatsStore(&frontend->counters[localFrontendSubmittedMessages], snapshot.attempts);
    localStatsStore(&frontend->counters[localFrontendSubmittedMax], snapshot.submitted_max);
    localStatsStore(&frontend->counters[localFrontendTerminalMessages], snapshot.terminal);
    localStatsStore(&frontend->counters[localFrontendOverflowMessages], snapshot.overflow);
    localStatsStore(&frontend->counters[localFrontendOverflowNoFitMessages], snapshot.nofit);
    localStatsStore(&frontend->counters[localFrontendOverflowOversizedMessages], snapshot.oversized);
    localStatsStore(&frontend->counters[localFrontendOverflowBatches], snapshot.overflow_batches);
    localStatsStore(&frontend->counters[localFrontendOversizedBatches], snapshot.oversized_batches);
    localStatsStore(&frontend->counters[localFrontendTransferToBeMessages], snapshot.transferred);
    localStatsStore(&frontend->counters[localFrontendShutdownDiscardedMessages], snapshot.shutdown_discarded);
    localStatsStore(&frontend->counters[localFrontendProducerExited], snapshot.producer_exited);
    localStatsStore(&frontend->counters[localFrontendDequeueBatches], snapshot.batches);
    localStatsStore(&frontend->counters[localFrontendDequeueMessages], snapshot.dequeue_messages);
    localStatsStore(&frontend->counters[localFrontendDequeueMax], snapshot.dequeue_max);
    localStatsStore(&frontend->counters[localFrontendBytes], snapshot.bytes);
    localStatsStore(&frontend->counters[localFrontendHelpAttempts], snapshot.help_attempts);
    localStatsStore(&frontend->counters[localFrontendHelpEmpty], snapshot.help_empty);
    localStatsStore(&frontend->counters[localFrontendHelpBatches], snapshot.help_batches);
    localStatsStore(&frontend->counters[localFrontendHelpMessages], snapshot.help_messages);
    localStatsStore(&frontend->counters[localFrontendHelpMax], snapshot.help_max);
    localStatsStore(&frontend->counters[localFrontendHelpActive], snapshot.help_active);
    localStatsStore(&frontend->counters[localFrontendHelpRetry], snapshot.help_retry);
    localStatsStore(&frontend->counters[localFrontendHelpTerminal], snapshot.help_terminal);
    localStatsStore(&frontend->counters[localFrontendHelpWaits], snapshot.help_waits);
    localStatsStore(&frontend->counters[localFrontendHelpWakes], snapshot.help_wakes);
    localStatsStore(&frontend->counters[localFrontendHelpLimit], snapshot.help_limit);
    localStatsStore(&frontend->counters[localFrontendHelpCompleted], snapshot.help_completed);
    localStatsStore(&frontend->counters[localFrontendHelpReturned], snapshot.help_returned);
    localStatsStore(&frontend->counters[localFrontendHelpWakeFe], snapshot.wake_fe);
    localStatsStore(&frontend->counters[localFrontendHelpWakeShutdown], snapshot.wake_shutdown);
}

static rsRetVal localStatsAddCounters(statsobj_t *const object,
                                      intctr_t *const counters,
                                      const localCounterDescriptor_t *const descriptors,
                                      const size_t count) {
    DEFiRet;
    for (size_t i = 0; i < count; ++i) {
        intctr_t *const storage = &counters[descriptors[i].index];
        PREFER_STORE_uint64(storage, 0);
        CHKiRet(
            statsobj.AddCounter(object, (const uchar *)descriptors[i].name, ctrType_IntCtr, CTR_FLAG_NONE, storage));
    }
finalize_it:
    RETiRet;
}

static rsRetVal localStatsConstructObject(statsobj_t **const out,
                                          const uchar *const name,
                                          void (*const callback)(statsobj_t *, void *),
                                          void *const context,
                                          intctr_t *const counters,
                                          const localCounterDescriptor_t *const descriptors,
                                          const size_t descriptor_count) {
    DEFiRet;
    CHKiRet(statsobj.Construct(out));
    CHKiRet(statsobj.SetName(*out, (uchar *)name));
    CHKiRet(statsobj.SetOrigin(*out, UCHAR_CONSTANT("core.queue.local")));
    CHKiRet(localStatsAddCounters(*out, counters, descriptors, descriptor_count));
    CHKiRet(statsobj.SetPreReadNotifier(*out, callback, context));
    CHKiRet(statsobj.ConstructFinalize(*out));
finalize_it:
    RETiRet;
}

void qqueueLocalStatsDestruct(qqueueLocalStats_t **const stats_ptr) {
    if (stats_ptr == NULL || *stats_ptr == NULL) return;
    qqueueLocalStats_t *const stats = *stats_ptr;
    if (stats->frontends != NULL) {
        for (uint32_t i = 0; i < stats->frontend_limit; ++i) {
            if (stats->frontends[i].object != NULL) statsobj.Destruct(&stats->frontends[i].object);
        }
    }
    if (stats->logical != NULL) statsobj.Destruct(&stats->logical);
    free(stats->frontends);
    free(stats);
    *stats_ptr = NULL;
}

#ifdef ENABLE_TESTBENCH
void qqueueLocalStatsSetTestPreReadHook(qqueueLocalStats_t *const stats,
                                        qqueueLocalStatsTestPreReadHook_t const hook,
                                        void *const context) {
    if (stats == NULL) return;
    stats->test_pre_read_hook = hook;
    stats->test_pre_read_context = context;
}

/* The test runs through imdiag after the daemon constructed a real local
 * queue. The hook is invoked by statsobj.GetAllCounters while the global
 * stats-object list mutex is held. A concurrent adapter destruction must
 * therefore remain blocked until the hook's condition is released. */
typedef struct localStatsLifetimePhase_s {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int pre_read_entered;
    int release_pre_read;
    int destroy_started;
    int destroy_returned;
    rsRetVal reader_status;
} localStatsLifetimePhase_t;

typedef struct localStatsLifetimeContext_s {
    qqueue_t *owner;
    localStatsLifetimePhase_t phase;
} localStatsLifetimeContext_t;

static void localStatsLifetimePreRead(void *const context) {
    localStatsLifetimeContext_t *const fixture = context;
    localStatsLifetimePhase_t *const phase = &fixture->phase;

    if (pthread_mutex_lock(&phase->mutex) != 0) return;
    phase->pre_read_entered = 1;
    pthread_cond_broadcast(&phase->condition);
    while (!phase->release_pre_read) pthread_cond_wait(&phase->condition, &phase->mutex);
    pthread_mutex_unlock(&phase->mutex);
}

static rsRetVal localStatsLifetimeCountLocal(void *const context,
                                             const uchar *const object_name __attribute__((unused)),
                                             const uchar *const object_origin,
                                             const uchar *const counter_name __attribute__((unused)),
                                             const statsCtrType_t counter_type __attribute__((unused)),
                                             const uint64_t value __attribute__((unused)),
                                             const int8_t flags __attribute__((unused))) {
    unsigned *const count = context;
    if (object_origin != NULL && !strcmp((const char *)object_origin, "core.queue.local")) ++*count;
    return RS_RET_OK;
}

static void *localStatsLifetimeRead(void *const context) {
    localStatsLifetimeContext_t *const fixture = context;
    fixture->phase.reader_status = statsobj.GetAllCounters(localStatsLifetimeCountLocal, &(unsigned){0});
    return NULL;
}

static void *localStatsLifetimeDestruct(void *const context) {
    localStatsLifetimeContext_t *const fixture = context;
    localStatsLifetimePhase_t *const phase = &fixture->phase;

    if (pthread_mutex_lock(&phase->mutex) == 0) {
        phase->destroy_started = 1;
        pthread_cond_broadcast(&phase->condition);
        pthread_mutex_unlock(&phase->mutex);
    }
    qqueueLocalStatsDestruct(&fixture->owner->localStats);
    if (pthread_mutex_lock(&phase->mutex) == 0) {
        phase->destroy_returned = 1;
        pthread_cond_broadcast(&phase->condition);
        pthread_mutex_unlock(&phase->mutex);
    }
    return NULL;
}

static rsRetVal localStatsLifetimeWait(localStatsLifetimePhase_t *const phase, int *const predicate) {
    int err = 0;

    if ((err = pthread_mutex_lock(&phase->mutex)) != 0) return RS_RET_CONC_CTRL_ERR;
    while (!*predicate && err == 0) err = pthread_cond_wait(&phase->condition, &phase->mutex);
    if (pthread_mutex_unlock(&phase->mutex) != 0) err = 1;
    return err == 0 ? RS_RET_OK : RS_RET_CONC_CTRL_ERR;
}

rsRetVal qqueueLocalStatsTestLifetime(qqueue_t *const owner) {
    localStatsLifetimeContext_t fixture = {.owner = owner};
    qqueueLocalStats_t *stats;
    pthread_t reader;
    pthread_t destructor;
    int mutex_initialized = 0;
    int condition_initialized = 0;
    int reader_started = 0;
    int destructor_started = 0;
    unsigned local_objects = 0;
    DEFiRet;

    if (owner == NULL || owner->localStats == NULL) return RS_RET_PARAM_ERROR;
    stats = owner->localStats;
    if (pthread_mutex_init(&fixture.phase.mutex, NULL) != 0) return RS_RET_CONC_CTRL_ERR;
    mutex_initialized = 1;
    if (pthread_cond_init(&fixture.phase.condition, NULL) != 0) {
        pthread_mutex_destroy(&fixture.phase.mutex);
        return RS_RET_CONC_CTRL_ERR;
    }
    condition_initialized = 1;
    qqueueLocalStatsSetTestPreReadHook(stats, localStatsLifetimePreRead, &fixture);
    CHKiConcCtrl(pthread_create(&reader, NULL, localStatsLifetimeRead, &fixture));
    reader_started = 1;
    CHKiRet(localStatsLifetimeWait(&fixture.phase, &fixture.phase.pre_read_entered));
    CHKiConcCtrl(pthread_create(&destructor, NULL, localStatsLifetimeDestruct, &fixture));
    destructor_started = 1;
    CHKiRet(localStatsLifetimeWait(&fixture.phase, &fixture.phase.destroy_started));

    CHKiConcCtrl(pthread_mutex_lock(&fixture.phase.mutex));
    if (fixture.phase.destroy_returned) {
        pthread_mutex_unlock(&fixture.phase.mutex);
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }
    fixture.phase.release_pre_read = 1;
    CHKiConcCtrl(pthread_cond_broadcast(&fixture.phase.condition));
    CHKiConcCtrl(pthread_mutex_unlock(&fixture.phase.mutex));

finalize_it:
    if (reader_started || destructor_started) {
        if (pthread_mutex_lock(&fixture.phase.mutex) == 0) {
            fixture.phase.release_pre_read = 1;
            pthread_cond_broadcast(&fixture.phase.condition);
            pthread_mutex_unlock(&fixture.phase.mutex);
        }
    }
    if (reader_started && pthread_join(reader, NULL) != 0 && iRet == RS_RET_OK) iRet = RS_RET_CONC_CTRL_ERR;
    if (destructor_started && pthread_join(destructor, NULL) != 0 && iRet == RS_RET_OK) iRet = RS_RET_CONC_CTRL_ERR;
    if (fixture.phase.reader_status != RS_RET_OK && iRet == RS_RET_OK) iRet = fixture.phase.reader_status;
    if (owner->localStats != NULL && iRet == RS_RET_OK) {
        qqueueLocalStatsSetTestPreReadHook(owner->localStats, NULL, NULL);
        iRet = RS_RET_INTERNAL_ERROR;
    }
    if (iRet == RS_RET_OK) {
        CHKiRet(statsobj.GetAllCounters(localStatsLifetimeCountLocal, &local_objects));
        if (local_objects != 0) iRet = RS_RET_INTERNAL_ERROR;
    }
    if (condition_initialized) pthread_cond_destroy(&fixture.phase.condition);
    if (mutex_initialized) pthread_mutex_destroy(&fixture.phase.mutex);
    RETiRet;
}
#endif

rsRetVal qqueueLocalStatsConstruct(qqueue_t *const owner,
                                   const uchar *const logical_name,
                                   const uint32_t frontend_limit,
                                   const int frontend_detail,
                                   qqueueLocalStats_t **const out) {
    DEFiRet;
    qqueueLocalStats_t *stats = NULL;
    char *object_name = NULL;

    if (owner == NULL || logical_name == NULL || out == NULL) return RS_RET_PARAM_ERROR;
    *out = NULL;
    CHKmalloc(stats = calloc(1, sizeof(*stats)));
    stats->owner = owner;
    stats->frontend_limit = frontend_detail ? frontend_limit : 0;
    if (asprintf(&object_name, "%s.local", logical_name) < 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    CHKiRet(localStatsConstructObject(&stats->logical, (const uchar *)object_name, localStatsPreRead, stats,
                                      stats->logical_counters, logicalCounters,
                                      sizeof(logicalCounters) / sizeof(logicalCounters[0])));
    free(object_name);
    object_name = NULL;

    if (stats->frontend_limit != 0) {
        CHKmalloc(stats->frontends = calloc(stats->frontend_limit, sizeof(*stats->frontends)));
        for (uint32_t i = 0; i < stats->frontend_limit; ++i) {
            localFrontendStats_t *const frontend = &stats->frontends[i];
            frontend->adapter = stats;
            frontend->index = i;
            if (asprintf(&object_name, "%s.local.frontend.%u", logical_name, i + 1) < 0)
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            CHKiRet(localStatsConstructObject(&frontend->object, (const uchar *)object_name, localFrontendStatsPreRead,
                                              frontend, frontend->counters, frontendCounters,
                                              sizeof(frontendCounters) / sizeof(frontendCounters[0])));
            free(object_name);
            object_name = NULL;
        }
    }
    *out = stats;
    stats = NULL;
finalize_it:
    free(object_name);
    qqueueLocalStatsDestruct(&stats);
    RETiRet;
}
