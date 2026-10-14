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

enum local_logical_counter {
    local_logical_sampled_out,
    local_logical_severity_discarded,
    local_logical_retry_failed,
    local_logical_reserved,
    local_logical_be_capacity,
    local_logical_fe_capacity,
    local_logical_fe_active_capacity,

    local_logical_ingress_messages,
    local_logical_accepted_messages,
    local_logical_rejected_preadmission_messages,
    local_logical_terminal_messages,
    local_logical_restored,
    local_logical_persisted,
    local_logical_executed,
    local_logical_discarded,
    local_logical_disk_transferred,
    local_logical_disk_terminal,
    local_logical_disk_physical,
    local_logical_disk_active,

    local_logical_terminal_shutdown_discarded_messages,
    local_logical_outstanding_messages,
    local_logical_route_fe_messages,
    local_logical_route_fe_batches,
    local_logical_route_be_messages,
    local_logical_route_be_batches,
    local_logical_route_be_accepted_messages,
    local_logical_be_terminal_messages,
    local_logical_route_be_no_fit_messages,
    local_logical_route_be_oversized_messages,
    local_logical_route_be_registration_fallback_messages,
    local_logical_route_be_shutdown_redirect_messages,
    local_logical_route_be_unclassified_messages,
    local_logical_route_be_internal_messages,
    local_logical_route_be_capacity_exhausted_messages,
    local_logical_be_physical_messages,
    local_logical_be_active_messages,
    local_logical_fe_queued_messages,
    local_logical_fe_active_messages,
    local_logical_fe_retry_messages,
    local_logical_fe_registered,
    local_logical_fe_started,
    local_logical_fe_producerless,
    local_logical_fe_registration_failures,
    local_logical_workers_fe,
    local_logical_workers_be,
    local_logical_configured_frontends,
    local_logical_transfer_fe_to_be_messages,
    local_logical_allocation_bytes,
    local_logical_be_dequeue_batches,
    local_logical_be_dequeue_messages,
    local_logical_be_dequeue_max,
    local_logical_help_attempts,
    local_logical_help_empty,
    local_logical_help_batches,
    local_logical_help_messages,
    local_logical_help_max,
    local_logical_help_active,
    local_logical_help_retry,
    local_logical_help_terminal,
    local_logical_help_waits,
    local_logical_help_wakes,
    local_logical_help_limit,
    local_logical_help_completed,
    local_logical_help_returned,
    local_logical_help_wake_fe,
    local_logical_help_wake_shutdown,
    local_logical_counter_count
};

enum local_frontend_counter {
    local_frontend_registration_id,
    local_frontend_generation,
    local_frontend_lifecycle_state,
    local_frontend_capacity,
    local_frontend_queued,
    local_frontend_free,
    local_frontend_inflight,
    local_frontend_retry,
    local_frontend_ingress_messages,
    local_frontend_admitted_messages,
    local_frontend_admitted_batches,
    local_frontend_submitted_batches,
    local_frontend_submitted_messages,
    local_frontend_submitted_max,
    local_frontend_terminal_messages,
    local_frontend_overflow_messages,
    local_frontend_overflow_no_fit_messages,
    local_frontend_overflow_oversized_messages,
    local_frontend_overflow_batches,
    local_frontend_oversized_batches,
    local_frontend_transfer_to_be_messages,
    local_frontend_shutdown_discarded_messages,
    local_frontend_producer_exited,
    local_frontend_dequeue_batches,
    local_frontend_dequeue_messages,
    local_frontend_dequeue_max,
    local_frontend_bytes,
    local_frontend_help_attempts,
    local_frontend_help_empty,
    local_frontend_help_batches,
    local_frontend_help_messages,
    local_frontend_help_max,
    local_frontend_help_active,
    local_frontend_help_retry,
    local_frontend_help_terminal,
    local_frontend_help_waits,
    local_frontend_help_wakes,
    local_frontend_help_limit,
    local_frontend_help_completed,
    local_frontend_help_returned,
    local_frontend_help_wake_fe,
    local_frontend_help_wake_shutdown,
    local_frontend_counter_count
};

typedef struct local_frontend_stats_s {
    struct qqueue_local_stats_s *adapter;
    uint32_t index;
    statsobj_t *object;
    intctr_t counters[local_frontend_counter_count];
} local_frontend_stats_t;

struct qqueue_local_stats_s {
    qqueue_t *owner;
    statsobj_t *logical;
    intctr_t logical_counters[local_logical_counter_count];
    uint32_t frontend_limit;
    local_frontend_stats_t *frontends;
#ifdef ENABLE_TESTBENCH
    qqueue_local_stats_test_pre_read_hook_t test_pre_read_hook;
    void *test_pre_read_context;
#endif
};

typedef struct local_counter_descriptor_s {
    const char *name;
    size_t index;
} local_counter_descriptor_t;

rsRetVal qqueue_local_stats_class_init(void) {
    DEFiRet;
    CHKiRet(objGetObjInterface(&obj));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
finalize_it:
    RETiRet;
}

#define LOCAL_COUNTER(name, member) \
    { name, member }
static const local_counter_descriptor_t logical_counters[] = {
    LOCAL_COUNTER("policy.sampled_out.messages", local_logical_sampled_out),
    LOCAL_COUNTER("policy.severity_discarded.messages", local_logical_severity_discarded),
    LOCAL_COUNTER("retry.enqueue_failed.messages", local_logical_retry_failed),
    LOCAL_COUNTER("resource.reserved.messages", local_logical_reserved),
    LOCAL_COUNTER("resource.be.capacity.messages", local_logical_be_capacity),
    LOCAL_COUNTER("resource.fe.capacity.messages", local_logical_fe_capacity),
    LOCAL_COUNTER("resource.fe.active.capacity.messages", local_logical_fe_active_capacity),

    LOCAL_COUNTER("ingress.messages", local_logical_ingress_messages),
    LOCAL_COUNTER("accepted.messages", local_logical_accepted_messages),
    LOCAL_COUNTER("rejected.preadmission.messages", local_logical_rejected_preadmission_messages),
    LOCAL_COUNTER("terminal.messages", local_logical_terminal_messages),
    LOCAL_COUNTER("restored.messages", local_logical_restored),
    LOCAL_COUNTER("terminal.persisted.messages", local_logical_persisted),
    LOCAL_COUNTER("terminal.executed.messages", local_logical_executed),
    LOCAL_COUNTER("terminal.discarded.messages", local_logical_discarded),
    LOCAL_COUNTER("transfer.be_to_disk.messages", local_logical_disk_transferred),
    LOCAL_COUNTER("disk.terminal.messages", local_logical_disk_terminal),
    LOCAL_COUNTER("disk.physical.messages", local_logical_disk_physical),
    LOCAL_COUNTER("disk.active.messages", local_logical_disk_active),

    LOCAL_COUNTER("terminal.shutdown_discarded.messages", local_logical_terminal_shutdown_discarded_messages),
    LOCAL_COUNTER("outstanding.messages", local_logical_outstanding_messages),
    LOCAL_COUNTER("route.fe.messages", local_logical_route_fe_messages),
    LOCAL_COUNTER("route.fe.batches", local_logical_route_fe_batches),
    LOCAL_COUNTER("route.be.messages", local_logical_route_be_messages),
    LOCAL_COUNTER("route.be.batches", local_logical_route_be_batches),
    LOCAL_COUNTER("route.be.accepted.messages", local_logical_route_be_accepted_messages),
    /* Physical backend terminal completions include FE-to-BE transfers. */
    LOCAL_COUNTER("be.terminal.messages", local_logical_be_terminal_messages),
    LOCAL_COUNTER("route.be.reason.nofit.messages", local_logical_route_be_no_fit_messages),
    LOCAL_COUNTER("route.be.reason.oversized.messages", local_logical_route_be_oversized_messages),
    LOCAL_COUNTER("route.be.reason.registration_fallback.messages",
                  local_logical_route_be_registration_fallback_messages),
    LOCAL_COUNTER("route.be.reason.shutdown_redirect.messages", local_logical_route_be_shutdown_redirect_messages),
    LOCAL_COUNTER("route.be.reason.unclassified.messages", local_logical_route_be_unclassified_messages),
    LOCAL_COUNTER("route.be.reason.internal.messages", local_logical_route_be_internal_messages),
    LOCAL_COUNTER("route.be.reason.capacity_exhausted.messages", local_logical_route_be_capacity_exhausted_messages),
    LOCAL_COUNTER("be.physical.messages", local_logical_be_physical_messages),
    LOCAL_COUNTER("be.active.messages", local_logical_be_active_messages),
    LOCAL_COUNTER("fe.queued.messages", local_logical_fe_queued_messages),
    LOCAL_COUNTER("fe.inflight.messages", local_logical_fe_active_messages),
    LOCAL_COUNTER("fe.retry.messages", local_logical_fe_retry_messages),
    LOCAL_COUNTER("fe.registered", local_logical_fe_registered),
    LOCAL_COUNTER("fe.started", local_logical_fe_started),
    LOCAL_COUNTER("fe.producerless", local_logical_fe_producerless),
    LOCAL_COUNTER("fe.registration_failures", local_logical_fe_registration_failures),
    LOCAL_COUNTER("workers.fe", local_logical_workers_fe),
    LOCAL_COUNTER("workers.be", local_logical_workers_be),
    LOCAL_COUNTER("capacity.frontends", local_logical_configured_frontends),
    LOCAL_COUNTER("transfer.fe_to_be.messages", local_logical_transfer_fe_to_be_messages),
    LOCAL_COUNTER("allocation.bytes", local_logical_allocation_bytes),
    LOCAL_COUNTER("batch.be_dequeue.count", local_logical_be_dequeue_batches),
    LOCAL_COUNTER("batch.be_dequeue.messages.sum", local_logical_be_dequeue_messages),
    LOCAL_COUNTER("batch.be_dequeue.messages.max", local_logical_be_dequeue_max),
    LOCAL_COUNTER("help.attempts", local_logical_help_attempts),
    LOCAL_COUNTER("help.empty", local_logical_help_empty),
    LOCAL_COUNTER("batch.help.count", local_logical_help_batches),
    LOCAL_COUNTER("batch.help.messages.sum", local_logical_help_messages),
    LOCAL_COUNTER("batch.help.messages.max", local_logical_help_max),
    LOCAL_COUNTER("inflight.help", local_logical_help_active),
    LOCAL_COUNTER("retry.help", local_logical_help_retry),
    LOCAL_COUNTER("terminal.help", local_logical_help_terminal),
    LOCAL_COUNTER("help.waits", local_logical_help_waits),
    LOCAL_COUNTER("help.wakes", local_logical_help_wakes),
    LOCAL_COUNTER("batch.help.limit", local_logical_help_limit),
    LOCAL_COUNTER("help.completed", local_logical_help_completed),
    LOCAL_COUNTER("help.returned", local_logical_help_returned),
    LOCAL_COUNTER("wake.fe", local_logical_help_wake_fe),
    LOCAL_COUNTER("wake.shutdown", local_logical_help_wake_shutdown),
};
#undef LOCAL_COUNTER

#define FRONTEND_COUNTER(name, member) \
    { name, member }
static const local_counter_descriptor_t frontend_counters[] = {
    FRONTEND_COUNTER("registration.id", local_frontend_registration_id),
    FRONTEND_COUNTER("registration.generation", local_frontend_generation),
    FRONTEND_COUNTER("lifecycle.state", local_frontend_lifecycle_state),
    FRONTEND_COUNTER("capacity", local_frontend_capacity),
    FRONTEND_COUNTER("queued", local_frontend_queued),
    FRONTEND_COUNTER("free", local_frontend_free),
    FRONTEND_COUNTER("inflight.fe", local_frontend_inflight),
    FRONTEND_COUNTER("retry.fe", local_frontend_retry),
    FRONTEND_COUNTER("ingress.messages", local_frontend_ingress_messages),
    FRONTEND_COUNTER("admitted.messages", local_frontend_admitted_messages),
    FRONTEND_COUNTER("admitted.batches", local_frontend_admitted_batches),
    FRONTEND_COUNTER("batch.submit.count", local_frontend_submitted_batches),
    FRONTEND_COUNTER("batch.submit.messages.sum", local_frontend_submitted_messages),
    FRONTEND_COUNTER("batch.submit.messages.max", local_frontend_submitted_max),
    FRONTEND_COUNTER("terminal.messages", local_frontend_terminal_messages),
    FRONTEND_COUNTER("overflow.messages", local_frontend_overflow_messages),
    FRONTEND_COUNTER("overflow.nofit.messages", local_frontend_overflow_no_fit_messages),
    FRONTEND_COUNTER("overflow.oversized.messages", local_frontend_overflow_oversized_messages),
    FRONTEND_COUNTER("overflow.batches", local_frontend_overflow_batches),
    FRONTEND_COUNTER("overflow.oversized_batches", local_frontend_oversized_batches),
    FRONTEND_COUNTER("transfer.fe_to_be.messages", local_frontend_transfer_to_be_messages),
    FRONTEND_COUNTER("shutdown.discarded.messages", local_frontend_shutdown_discarded_messages),
    FRONTEND_COUNTER("producer.exited", local_frontend_producer_exited),
    FRONTEND_COUNTER("batch.fe_dequeue.count", local_frontend_dequeue_batches),
    FRONTEND_COUNTER("batch.fe_dequeue.messages.sum", local_frontend_dequeue_messages),
    FRONTEND_COUNTER("batch.fe_dequeue.messages.max", local_frontend_dequeue_max),
    FRONTEND_COUNTER("admitted.raw_bytes", local_frontend_bytes),
    FRONTEND_COUNTER("help.attempts", local_frontend_help_attempts),
    FRONTEND_COUNTER("help.empty", local_frontend_help_empty),
    FRONTEND_COUNTER("batch.help.count", local_frontend_help_batches),
    FRONTEND_COUNTER("batch.help.messages.sum", local_frontend_help_messages),
    FRONTEND_COUNTER("batch.help.messages.max", local_frontend_help_max),
    FRONTEND_COUNTER("inflight.help", local_frontend_help_active),
    FRONTEND_COUNTER("retry.help", local_frontend_help_retry),
    FRONTEND_COUNTER("terminal.help", local_frontend_help_terminal),
    FRONTEND_COUNTER("help.waits", local_frontend_help_waits),
    FRONTEND_COUNTER("help.wakes", local_frontend_help_wakes),
    FRONTEND_COUNTER("batch.help.limit", local_frontend_help_limit),
    FRONTEND_COUNTER("help.completed", local_frontend_help_completed),
    FRONTEND_COUNTER("help.returned", local_frontend_help_returned),
    FRONTEND_COUNTER("wake.fe", local_frontend_help_wake_fe),
    FRONTEND_COUNTER("wake.shutdown", local_frontend_help_wake_shutdown),
};
#undef FRONTEND_COUNTER

static void local_stats_store(intctr_t *const counter, const uint64_t value) {
    PREFER_STORE_uint64(counter, value);
}

static void local_stats_run_test_pre_read_hook(qqueue_local_stats_t *const stats) {
#ifdef ENABLE_TESTBENCH
    if (stats->test_pre_read_hook != NULL) stats->test_pre_read_hook(stats->test_pre_read_context);
#else
    (void)stats;
#endif
}

static void local_stats_pre_read(statsobj_t *const object, void *const context) {
    (void)object;
    qqueue_local_stats_t *const stats = context;
    qqueueLocalSnapshot_t snapshot;

    local_stats_run_test_pre_read_hook(stats);
    qqueueLocalGetSnapshot(stats->owner, &snapshot);
    local_stats_store(&stats->logical_counters[local_logical_sampled_out], snapshot.sampled_out);
    local_stats_store(&stats->logical_counters[local_logical_severity_discarded], snapshot.severity_discarded);
    local_stats_store(&stats->logical_counters[local_logical_retry_failed], snapshot.retry_failed);
    local_stats_store(&stats->logical_counters[local_logical_reserved], snapshot.reserved_messages);
    local_stats_store(&stats->logical_counters[local_logical_be_capacity], snapshot.be_capacity);
    local_stats_store(&stats->logical_counters[local_logical_fe_capacity], snapshot.fe_capacity);
    local_stats_store(&stats->logical_counters[local_logical_fe_active_capacity], snapshot.fe_active_capacity);

    local_stats_store(&stats->logical_counters[local_logical_ingress_messages], snapshot.attempts);
    local_stats_store(&stats->logical_counters[local_logical_accepted_messages], snapshot.admitted);
    local_stats_store(&stats->logical_counters[local_logical_rejected_preadmission_messages],
                      snapshot.preadmission_rejected);
    local_stats_store(&stats->logical_counters[local_logical_terminal_messages], snapshot.terminal);
    local_stats_store(&stats->logical_counters[local_logical_restored], snapshot.restored);
    local_stats_store(&stats->logical_counters[local_logical_persisted], snapshot.persisted);
    local_stats_store(&stats->logical_counters[local_logical_executed], snapshot.executed);
    local_stats_store(&stats->logical_counters[local_logical_discarded], snapshot.discarded);
    local_stats_store(&stats->logical_counters[local_logical_disk_transferred], snapshot.disk_transferred);
    local_stats_store(&stats->logical_counters[local_logical_disk_terminal], snapshot.disk_terminal);
    local_stats_store(&stats->logical_counters[local_logical_disk_physical], snapshot.disk_physical);
    local_stats_store(&stats->logical_counters[local_logical_disk_active], snapshot.disk_active);

    local_stats_store(&stats->logical_counters[local_logical_terminal_shutdown_discarded_messages],
                      snapshot.shutdown_discarded);
    local_stats_store(&stats->logical_counters[local_logical_outstanding_messages], snapshot.outstanding);
    local_stats_store(&stats->logical_counters[local_logical_route_fe_messages], snapshot.route_fe_messages);
    local_stats_store(&stats->logical_counters[local_logical_route_fe_batches], snapshot.route_fe_batches);
    local_stats_store(&stats->logical_counters[local_logical_route_be_messages], snapshot.route_be_messages);
    local_stats_store(&stats->logical_counters[local_logical_route_be_batches], snapshot.route_be_batches);
    local_stats_store(&stats->logical_counters[local_logical_route_be_accepted_messages], snapshot.be_admitted);
    local_stats_store(&stats->logical_counters[local_logical_be_terminal_messages], snapshot.be_terminal);
    local_stats_store(&stats->logical_counters[local_logical_route_be_no_fit_messages], snapshot.be_nofit);
    local_stats_store(&stats->logical_counters[local_logical_route_be_oversized_messages], snapshot.be_oversized);
    local_stats_store(&stats->logical_counters[local_logical_route_be_registration_fallback_messages],
                      snapshot.be_registration_fallback);
    local_stats_store(&stats->logical_counters[local_logical_route_be_shutdown_redirect_messages],
                      snapshot.be_shutdown_redirect);
    local_stats_store(&stats->logical_counters[local_logical_route_be_unclassified_messages], snapshot.be_unclassified);
    local_stats_store(&stats->logical_counters[local_logical_route_be_internal_messages], snapshot.be_internal);
    local_stats_store(&stats->logical_counters[local_logical_route_be_capacity_exhausted_messages],
                      snapshot.be_capacity_exhausted);
    local_stats_store(&stats->logical_counters[local_logical_be_physical_messages], snapshot.be_physical);
    local_stats_store(&stats->logical_counters[local_logical_be_active_messages], snapshot.be_active);
    local_stats_store(&stats->logical_counters[local_logical_fe_queued_messages], snapshot.fe_queued);
    local_stats_store(&stats->logical_counters[local_logical_fe_active_messages], snapshot.fe_active);
    local_stats_store(&stats->logical_counters[local_logical_fe_retry_messages], snapshot.fe_retry);
    local_stats_store(&stats->logical_counters[local_logical_fe_registered], snapshot.fe_registered);
    local_stats_store(&stats->logical_counters[local_logical_fe_started], snapshot.fe_started);
    local_stats_store(&stats->logical_counters[local_logical_fe_producerless], snapshot.fe_producerless);
    local_stats_store(&stats->logical_counters[local_logical_fe_registration_failures], snapshot.registration_failures);
    local_stats_store(&stats->logical_counters[local_logical_workers_fe], snapshot.fe_consumers);
    local_stats_store(&stats->logical_counters[local_logical_workers_be], snapshot.be_consumers);
    local_stats_store(&stats->logical_counters[local_logical_configured_frontends], snapshot.configured_frontends);
    local_stats_store(&stats->logical_counters[local_logical_transfer_fe_to_be_messages], snapshot.transferred);
    local_stats_store(&stats->logical_counters[local_logical_allocation_bytes], snapshot.allocation_bytes);
    local_stats_store(&stats->logical_counters[local_logical_be_dequeue_batches], snapshot.be_dequeue_batches);
    local_stats_store(&stats->logical_counters[local_logical_be_dequeue_messages], snapshot.be_dequeue_messages);
    local_stats_store(&stats->logical_counters[local_logical_be_dequeue_max], snapshot.be_dequeue_max);
    local_stats_store(&stats->logical_counters[local_logical_help_attempts], snapshot.help_attempts);
    local_stats_store(&stats->logical_counters[local_logical_help_empty], snapshot.help_empty);
    local_stats_store(&stats->logical_counters[local_logical_help_batches], snapshot.help_batches);
    local_stats_store(&stats->logical_counters[local_logical_help_messages], snapshot.help_messages);
    local_stats_store(&stats->logical_counters[local_logical_help_max], snapshot.help_max);
    local_stats_store(&stats->logical_counters[local_logical_help_active], snapshot.help_active);
    local_stats_store(&stats->logical_counters[local_logical_help_retry], snapshot.help_retry);
    local_stats_store(&stats->logical_counters[local_logical_help_terminal], snapshot.help_terminal);
    local_stats_store(&stats->logical_counters[local_logical_help_waits], snapshot.help_waits);
    local_stats_store(&stats->logical_counters[local_logical_help_wakes], snapshot.help_wakes);
    local_stats_store(&stats->logical_counters[local_logical_help_limit], snapshot.help_limit);
    local_stats_store(&stats->logical_counters[local_logical_help_completed], snapshot.help_completed);
    local_stats_store(&stats->logical_counters[local_logical_help_returned], snapshot.help_returned);
    local_stats_store(&stats->logical_counters[local_logical_help_wake_fe], snapshot.wake_fe);
    local_stats_store(&stats->logical_counters[local_logical_help_wake_shutdown], snapshot.wake_shutdown);
}

static void local_frontend_stats_pre_read(statsobj_t *const object, void *const context) {
    (void)object;
    local_frontend_stats_t *const frontend = context;
    qqueueLocalFrontendSnapshot_t snapshot;

    local_stats_run_test_pre_read_hook(frontend->adapter);
    if (!qqueueLocalGetFrontendSnapshot(frontend->adapter->owner, frontend->index, &snapshot)) return;
    local_stats_store(&frontend->counters[local_frontend_registration_id], snapshot.identity);
    local_stats_store(&frontend->counters[local_frontend_generation], snapshot.generation);
    local_stats_store(&frontend->counters[local_frontend_lifecycle_state], snapshot.state);
    local_stats_store(&frontend->counters[local_frontend_capacity], snapshot.capacity);
    local_stats_store(&frontend->counters[local_frontend_queued], snapshot.queued);
    local_stats_store(&frontend->counters[local_frontend_free],
                      snapshot.capacity >= snapshot.queued ? snapshot.capacity - snapshot.queued : 0);
    local_stats_store(&frontend->counters[local_frontend_inflight], snapshot.active);
    local_stats_store(&frontend->counters[local_frontend_retry], snapshot.retry);
    local_stats_store(&frontend->counters[local_frontend_ingress_messages], snapshot.attempts);
    local_stats_store(&frontend->counters[local_frontend_admitted_messages], snapshot.published);
    local_stats_store(&frontend->counters[local_frontend_admitted_batches], snapshot.published_batches);
    local_stats_store(&frontend->counters[local_frontend_submitted_batches], snapshot.submitted_batches);
    local_stats_store(&frontend->counters[local_frontend_submitted_messages], snapshot.attempts);
    local_stats_store(&frontend->counters[local_frontend_submitted_max], snapshot.submitted_max);
    local_stats_store(&frontend->counters[local_frontend_terminal_messages], snapshot.terminal);
    local_stats_store(&frontend->counters[local_frontend_overflow_messages], snapshot.overflow);
    local_stats_store(&frontend->counters[local_frontend_overflow_no_fit_messages], snapshot.nofit);
    local_stats_store(&frontend->counters[local_frontend_overflow_oversized_messages], snapshot.oversized);
    local_stats_store(&frontend->counters[local_frontend_overflow_batches], snapshot.overflow_batches);
    local_stats_store(&frontend->counters[local_frontend_oversized_batches], snapshot.oversized_batches);
    local_stats_store(&frontend->counters[local_frontend_transfer_to_be_messages], snapshot.transferred);
    local_stats_store(&frontend->counters[local_frontend_shutdown_discarded_messages], snapshot.shutdown_discarded);
    local_stats_store(&frontend->counters[local_frontend_producer_exited], snapshot.producer_exited);
    local_stats_store(&frontend->counters[local_frontend_dequeue_batches], snapshot.batches);
    local_stats_store(&frontend->counters[local_frontend_dequeue_messages], snapshot.dequeue_messages);
    local_stats_store(&frontend->counters[local_frontend_dequeue_max], snapshot.dequeue_max);
    local_stats_store(&frontend->counters[local_frontend_bytes], snapshot.bytes);
    local_stats_store(&frontend->counters[local_frontend_help_attempts], snapshot.help_attempts);
    local_stats_store(&frontend->counters[local_frontend_help_empty], snapshot.help_empty);
    local_stats_store(&frontend->counters[local_frontend_help_batches], snapshot.help_batches);
    local_stats_store(&frontend->counters[local_frontend_help_messages], snapshot.help_messages);
    local_stats_store(&frontend->counters[local_frontend_help_max], snapshot.help_max);
    local_stats_store(&frontend->counters[local_frontend_help_active], snapshot.help_active);
    local_stats_store(&frontend->counters[local_frontend_help_retry], snapshot.help_retry);
    local_stats_store(&frontend->counters[local_frontend_help_terminal], snapshot.help_terminal);
    local_stats_store(&frontend->counters[local_frontend_help_waits], snapshot.help_waits);
    local_stats_store(&frontend->counters[local_frontend_help_wakes], snapshot.help_wakes);
    local_stats_store(&frontend->counters[local_frontend_help_limit], snapshot.help_limit);
    local_stats_store(&frontend->counters[local_frontend_help_completed], snapshot.help_completed);
    local_stats_store(&frontend->counters[local_frontend_help_returned], snapshot.help_returned);
    local_stats_store(&frontend->counters[local_frontend_help_wake_fe], snapshot.wake_fe);
    local_stats_store(&frontend->counters[local_frontend_help_wake_shutdown], snapshot.wake_shutdown);
}

static rsRetVal local_stats_add_counters(statsobj_t *const object,
                                         intctr_t *const counters,
                                         const local_counter_descriptor_t *const descriptors,
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

static rsRetVal local_stats_construct_object(statsobj_t **const out,
                                             const uchar *const name,
                                             void (*const callback)(statsobj_t *, void *),
                                             void *const context,
                                             intctr_t *const counters,
                                             const local_counter_descriptor_t *const descriptors,
                                             const size_t descriptor_count) {
    DEFiRet;
    CHKiRet(statsobj.Construct(out));
    CHKiRet(statsobj.SetName(*out, (uchar *)name));
    CHKiRet(statsobj.SetOrigin(*out, UCHAR_CONSTANT("core.queue.local")));
    CHKiRet(local_stats_add_counters(*out, counters, descriptors, descriptor_count));
    CHKiRet(statsobj.SetPreReadNotifier(*out, callback, context));
    CHKiRet(statsobj.ConstructFinalize(*out));
finalize_it:
    RETiRet;
}

void qqueue_local_stats_destruct(qqueue_local_stats_t **const stats_ptr) {
    if (stats_ptr == NULL || *stats_ptr == NULL) return;
    qqueue_local_stats_t *const stats = *stats_ptr;
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
void qqueue_local_stats_set_test_pre_read_hook(qqueue_local_stats_t *const stats,
                                               qqueue_local_stats_test_pre_read_hook_t const hook,
                                               void *const context) {
    if (stats == NULL) return;
    stats->test_pre_read_hook = hook;
    stats->test_pre_read_context = context;
}

/* The test runs through imdiag after the daemon constructed a real local
 * queue. The hook is invoked by statsobj.GetAllCounters while the global
 * stats-object list mutex is held. A concurrent adapter destruction must
 * therefore remain blocked until the hook's condition is released. */
typedef struct local_stats_lifetime_phase_s {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int pre_read_entered;
    int release_pre_read;
    int destroy_started;
    int destroy_returned;
    rsRetVal reader_status;
} local_stats_lifetime_phase_t;

typedef struct local_stats_lifetime_context_s {
    qqueue_t *owner;
    local_stats_lifetime_phase_t phase;
} local_stats_lifetime_context_t;

static void local_stats_lifetime_pre_read(void *const context) {
    local_stats_lifetime_context_t *const fixture = context;
    local_stats_lifetime_phase_t *const phase = &fixture->phase;

    if (pthread_mutex_lock(&phase->mutex) != 0) return;
    phase->pre_read_entered = 1;
    pthread_cond_broadcast(&phase->condition);
    while (!phase->release_pre_read) pthread_cond_wait(&phase->condition, &phase->mutex);
    pthread_mutex_unlock(&phase->mutex);
}

static rsRetVal local_stats_lifetime_count_local(void *const context,
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

static void *local_stats_lifetime_read(void *const context) {
    local_stats_lifetime_context_t *const fixture = context;
    fixture->phase.reader_status = statsobj.GetAllCounters(local_stats_lifetime_count_local, &(unsigned){0});
    return NULL;
}

static void *local_stats_lifetime_destruct(void *const context) {
    local_stats_lifetime_context_t *const fixture = context;
    local_stats_lifetime_phase_t *const phase = &fixture->phase;

    if (pthread_mutex_lock(&phase->mutex) == 0) {
        phase->destroy_started = 1;
        pthread_cond_broadcast(&phase->condition);
        pthread_mutex_unlock(&phase->mutex);
    }
    qqueue_local_stats_destruct(&fixture->owner->local_stats);
    if (pthread_mutex_lock(&phase->mutex) == 0) {
        phase->destroy_returned = 1;
        pthread_cond_broadcast(&phase->condition);
        pthread_mutex_unlock(&phase->mutex);
    }
    return NULL;
}

static rsRetVal local_stats_lifetime_wait(local_stats_lifetime_phase_t *const phase, int *const predicate) {
    int err = 0;

    if ((err = pthread_mutex_lock(&phase->mutex)) != 0) return RS_RET_CONC_CTRL_ERR;
    while (!*predicate && err == 0) err = pthread_cond_wait(&phase->condition, &phase->mutex);
    if (pthread_mutex_unlock(&phase->mutex) != 0) err = 1;
    return err == 0 ? RS_RET_OK : RS_RET_CONC_CTRL_ERR;
}

rsRetVal qqueue_local_stats_test_lifetime(qqueue_t *const owner) {
    local_stats_lifetime_context_t fixture = {.owner = owner};
    qqueue_local_stats_t *stats;
    pthread_t reader;
    pthread_t destructor;
    int mutex_initialized = 0;
    int condition_initialized = 0;
    int reader_started = 0;
    int destructor_started = 0;
    unsigned local_objects = 0;
    DEFiRet;

    if (owner == NULL || owner->local_stats == NULL) return RS_RET_PARAM_ERROR;
    stats = owner->local_stats;
    if (pthread_mutex_init(&fixture.phase.mutex, NULL) != 0) return RS_RET_CONC_CTRL_ERR;
    mutex_initialized = 1;
    if (pthread_cond_init(&fixture.phase.condition, NULL) != 0) {
        pthread_mutex_destroy(&fixture.phase.mutex);
        return RS_RET_CONC_CTRL_ERR;
    }
    condition_initialized = 1;
    qqueue_local_stats_set_test_pre_read_hook(stats, local_stats_lifetime_pre_read, &fixture);
    CHKiConcCtrl(pthread_create(&reader, NULL, local_stats_lifetime_read, &fixture));
    reader_started = 1;
    CHKiRet(local_stats_lifetime_wait(&fixture.phase, &fixture.phase.pre_read_entered));
    CHKiConcCtrl(pthread_create(&destructor, NULL, local_stats_lifetime_destruct, &fixture));
    destructor_started = 1;
    CHKiRet(local_stats_lifetime_wait(&fixture.phase, &fixture.phase.destroy_started));

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
    if (owner->local_stats != NULL && iRet == RS_RET_OK) {
        qqueue_local_stats_set_test_pre_read_hook(owner->local_stats, NULL, NULL);
        iRet = RS_RET_INTERNAL_ERROR;
    }
    if (iRet == RS_RET_OK) {
        CHKiRet(statsobj.GetAllCounters(local_stats_lifetime_count_local, &local_objects));
        if (local_objects != 0) iRet = RS_RET_INTERNAL_ERROR;
    }
    if (condition_initialized) pthread_cond_destroy(&fixture.phase.condition);
    if (mutex_initialized) pthread_mutex_destroy(&fixture.phase.mutex);
    RETiRet;
}
#endif

rsRetVal qqueue_local_stats_construct(qqueue_t *const owner,
                                      const uchar *const logical_name,
                                      const uint32_t frontend_limit,
                                      const int frontend_detail,
                                      qqueue_local_stats_t **const out) {
    DEFiRet;
    qqueue_local_stats_t *stats = NULL;
    char *object_name = NULL;

    if (owner == NULL || logical_name == NULL || out == NULL) return RS_RET_PARAM_ERROR;
    *out = NULL;
    CHKmalloc(stats = calloc(1, sizeof(*stats)));
    stats->owner = owner;
    stats->frontend_limit = frontend_detail ? frontend_limit : 0;
    if (asprintf(&object_name, "%s.local", logical_name) < 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    CHKiRet(local_stats_construct_object(&stats->logical, (const uchar *)object_name, local_stats_pre_read, stats,
                                         stats->logical_counters, logical_counters,
                                         sizeof(logical_counters) / sizeof(logical_counters[0])));
    free(object_name);
    object_name = NULL;

    if (stats->frontend_limit != 0) {
        CHKmalloc(stats->frontends = calloc(stats->frontend_limit, sizeof(*stats->frontends)));
        for (uint32_t i = 0; i < stats->frontend_limit; ++i) {
            local_frontend_stats_t *const frontend = &stats->frontends[i];
            frontend->adapter = stats;
            frontend->index = i;
            if (asprintf(&object_name, "%s.local.frontend.%u", logical_name, i + 1) < 0)
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            CHKiRet(local_stats_construct_object(
                &frontend->object, (const uchar *)object_name, local_frontend_stats_pre_read, frontend,
                frontend->counters, frontend_counters, sizeof(frontend_counters) / sizeof(frontend_counters[0])));
            free(object_name);
            object_name = NULL;
        }
    }
    *out = stats;
    stats = NULL;
finalize_it:
    free(object_name);
    qqueue_local_stats_destruct(&stats);
    RETiRet;
}
