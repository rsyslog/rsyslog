/* Experimental local queue integration. Internal API, not a module ABI.
 *
 * Concurrency & Locking: FE state is bounded by configuration and retained
 * until logical queue destruction. Message ownership is transferred only by
 * successful ring publication, source completion, or explicit BE admission.
 */
#ifndef QUEUE_LOCAL_H_INCLUDED
#define QUEUE_LOCAL_H_INCLUDED

#include <stdint.h>
#include <time.h>
#include "rsyslog.h"
#include "typedefs.h"

typedef struct qqueueLocal_s qqueueLocal_t;
typedef struct qqueueLocalFrontend_s qqueueLocalFrontend_t;

typedef struct qqueueLocalFrontendSnapshot_s {
    uint64_t identity, capacity, attempts, published, dequeued, terminal;
    uint64_t active, retry, overflow, nofit, oversized, transferred, shutdown_discarded;
    uint64_t queued, bytes, batches, producer_exited;
    uint64_t generation, published_batches, dequeue_max, dequeue_messages;
    uint64_t submitted_batches, submitted_max, overflow_batches, oversized_batches;
    unsigned state;
} qqueueLocalFrontendSnapshot_t;

typedef struct qqueueLocalSnapshot_s {
    uint64_t attempts, admitted, preadmission_rejected, terminal, shutdown_discarded;
    uint64_t be_attempted, be_admitted, be_terminal, be_physical, be_active;
    uint64_t fe_queued, fe_active, fe_retry, fe_registered, fe_started, fe_producerless;
    uint64_t registration_failures, transferred, allocation_bytes;
    uint64_t configured_frontends, outstanding, route_fe_messages, route_fe_batches, route_be_messages,
        route_be_batches;
    uint64_t be_nofit, be_oversized, be_registration_fallback, be_shutdown_redirect, be_unclassified;
    uint64_t fe_consumers, be_consumers;
    uint64_t be_internal, be_capacity_exhausted, capacity_exhaustions;
    uint64_t be_dequeue_batches, be_dequeue_messages, be_dequeue_max;
} qqueueLocalSnapshot_t;

/* Called only by actual tcpsrv execution, never inferred from message inputname.
 * Exit runs before input-thread joins permit queue destruction. The TLS key's
 * fallback destructor frees cache nodes without dereferencing queue storage. */
int qqueueLocalEnabled(void);
void qqueueLocalProducerEnter(void);
void qqueueLocalProducerLeave(void);
void qqueueLocalProducerExit(void *unused);
#ifdef ENABLE_TESTBENCH
/* Deterministic test controls; absent from production builds. */
int qqueueLocalTestProducerShouldExit(void);
rsRetVal qqueueLocalTestArmShutdownCheck(qqueue_t *owner, const char *markerPath);
void qqueueLocalTestRedirectArm(void);
int qqueueLocalTestRedirectWaitPublisher(unsigned timeout_ms);
void qqueueLocalTestRedirectRelease(void);
void qqueueLocalTestNoteForceTerm(wti_t *worker, int currentIParams);
#endif


rsRetVal qqueueLocalStart(qqueue_t *owner);
rsRetVal qqueueLocalSubmit(qqueue_t *owner, smsg_t *const *messages, size_t count, int single_flow_control);
rsRetVal qqueueLocalShutdown(qqueue_t *owner);
void qqueueLocalDestruct(qqueue_t *owner);
int qqueueLocalIsClosed(const qqueue_t *owner);
int qqueueLocalWorker(const wti_t *worker);
/* Caller owns the physical source mutex. Preserve DISC; COMM is ambiguous
 * on interrupted callbacks and must remain an accepted obligation. */
void qqueueLocalRetainAmbiguous(wti_t *worker);
void qqueueLocalRefreshLegacy(qqueue_t *owner);
void qqueueLocalGetSnapshot(const qqueue_t *owner, qqueueLocalSnapshot_t *snapshot);
int qqueueLocalGetFrontendSnapshot(const qqueue_t *owner, uint32_t index, qqueueLocalFrontendSnapshot_t *snapshot);

/* BE accounting calls are serialized by owner->mut; snapshot readers use
 * atomic loads only. These are lifetime totals, not resettable impstats data. */
void qqueueLocalBackendAcquired(qqueue_t *owner, uint64_t count);
void qqueueLocalBackendAttempt(qqueue_t *owner, uint64_t count);
void qqueueLocalBackendAdmitted(qqueue_t *owner, uint64_t count);
void qqueueLocalBackendRejected(qqueue_t *owner, uint64_t count);
void qqueueLocalBackendTerminal(qqueue_t *owner, uint64_t count, uint64_t discarded);
void qqueueLocalBackendBegin(qqueue_t *owner);
void qqueueLocalBackendEnd(qqueue_t *owner);
int qqueueLocalBackendWaitSpace(qqueue_t *owner, const struct timespec *deadline);
void qqueueLocalBackendWakeSpace(qqueue_t *owner);

enum qqueueLocalRouteReason {
    QLOCAL_NOFIT,
    QLOCAL_OVERSIZED,
    QLOCAL_REGISTRATION,
    QLOCAL_REDIRECT,
    QLOCAL_UNCLASSIFIED,
    QLOCAL_INTERNAL,
    QLOCAL_CAPACITY
};
void qqueueLocalBackendRoute(qqueue_t *owner, size_t count, enum qqueueLocalRouteReason reason);

/* Implemented in queue.c beside FixedArray's private storage handlers.
 * External submission consumes every supplied reference, including FORCE_TERM.
 * Internal transfer either takes the one supplied reference or retains it. */
rsRetVal qqueueLocalSubmitBackend(qqueue_t *owner,
                                  smsg_t *const *messages,
                                  size_t count,
                                  int single_flow_control,
                                  enum qqueueLocalRouteReason reason);
rsRetVal qqueueLocalTransferBackend(qqueue_t *owner, smsg_t *message, const struct timespec *deadline);
void qqueueLocalDiscardBackend(qqueue_t *owner);

#endif
