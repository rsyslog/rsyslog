<!--
.. meta::
   :description: S2 worker integration rationale and refinements for bounded memory-only local queue front ends.
   :keywords: rsyslog, local queue, S2, SPSC, worker, lease, cancellation, shutdown
-->

<a id="local-queue-s2-integration-notes"></a>
# Local queue S2 worker integration notes

| Metadata | Value |
|---|---|
| Status | **Implemented MVP; final qualification pending (see execution ledger)** |
| Recorded | 2026-09-14 |
| Note branch baseline | `8730dd23eeccac83c1772e6003bea5da148cab5b` |
| Runtime audited | `b0d9f971f007f06db3f734543cef5dfb312c5090` |
| Authority | [Design](local-queue-frontends.md), [implementation plan](local-queue-implementation-plan.md), [ownership audit](local-queue-s0-ownership.md) |

<!-- .. summary-start -->
Reuse one existing worker pool per FE, with private source identity,
bounded leases, FE-local notification and coordinated shutdown.
The original proposal and implementation refinements explain S2; the execution
ledger records current validation. This does not replace the overall design.
<!-- .. summary-end -->

## Implementation refinements after review

The MVP is integrated. These refinements supersede corresponding provisional
details below; they do not establish stage acceptance. See the
[execution ledger](local-queue-execution-ledger.md) for evidence and remaining gates.

- S1 source/mutex attribution is independently approved at c9a51be21. The
  attribution seam preserves inherited disk completion context but does not
  repair SC1 final orphan cleanup. S2 remains memory-only.
- COMM is not delivery proof: ruleset execution marks it before Direct action
  transactions commit. Local interrupted callbacks conservatively restore
  ambiguous COMM obligations before disposing private action state and source
  completion. Explicit discards remain terminal. Unknown external prefixes
  can produce duplicates and repeated mutations; no exactly-once promise.
- Qualified synchronous omfile uses a preopened regular append stream prepared
  during activation after privilege drop. Configuration checking performs no
  output creation. Cancellation discards pending stream bytes and unlocks the
  shared instance without I/O. Worker callbacks cannot lazily reopen or close
  this stream; main-thread HUP owns descriptor recovery. This does not make
  regular-file I/O bounded. Joining remains necessary before freeing callback
  state, even if the policy deadline has expired.
- Local cancellation requests must reach all FEs before any post-cancellation
  join. The legacy helper's per-worker sleep/join was unsuitable. The new
  local-only pool operation protects thread identity under the pool mutex,
  issues requests without logging or waits, then the family joins. Terminal
  publication and its condition signal remain under the same mutex; local
  queue-emitting lifecycle logging is omitted to avoid recursive admission.
- Snapshot collection preserves one logical queue while reporting BE physical
  size and explicit logical/FE inventories separately. FE submission count,
  actual sum and maximum distinguish submission shape from dequeue shape.
  Whole waves of TCP messages do not prove one equally sized enqueue batch.
- Deterministic tests must cover actual arrays at qqueueLocalSubmit, publication
  racing REDIRECT, failed registration/startup, producer exit with backlog and
  no slot reuse, exact quiescent ownership counters, and stats teardown/reset.
  Ring unit tests and normal TCP success tests do not replace these proofs.
- Latency is a separate same-process monotonic user-space observation. Exact
  output payload and timestamp manifests are checked after shutdown. Reader
  cadence includes the final parsing pass; framing preparation and dispatch
  lateness are distinct. The fixed 400us cadence limits are validity gates,
  not a fabricated additive uncertainty bound for opaque kernel/sendall work.
  Baseline-only feasibility must precede candidate latency acceptance.

The following numbered sections preserve the original implementation rationale
and its historical review context.

## 1. Review scope and accepted boundary

The coordinator reviewed this implementation recommendation during the S0
measurement session. The coordinator accepted an initial unconditional FE-local
mutex notification after batch publication, provided callbacks release that mutex
and the resulting implementation is measured. This is proposal review, not an
independent review of code or evidence of correctness or performance.

The companion working documents `local-queue-s0-contracts.md` and
`local-queue-execution-ledger.md`, read on 2026-09-14, supplied the approved O1-O12
choices. They were not yet tracked at this note's branch baseline. Integrate their
final revisions alongside these notes and recheck any changed contract before
coding. S1 supplies the source/logical-owner lease seam; S2 must not assume it
already exists in the runtime audited here.

Scope remains local main/ruleset queues, FixedArray BE, bounded preallocated FEs,
trusted imtcp producers and the approved callback/configuration whitelist. There
is no BE helping, disk/DA, action-queue FE, graph expansion or reload support.
Each registration has one actual producer and one dedicated consumer. Failed and
producerless registrations are not reused before logical queue destruction.

## 2. Private source adapter and worker allocation

Prefer one existing one-worker `wtp` per FE over bespoke pthreads. The existing
framework supplies signal masks, startup acknowledgement, cancellation handlers,
thread accounting and joins. A bespoke loop would need to reproduce those
contracts and the action-worker lifecycle.

Proposed internal field: `qqueue_t::pLocalFrontendSource`, NULL for ordinary
queues. The referenced FE descriptor owns its ring, mutex, counters, lifecycle
state and an explicit `logicalOwner`. Its queue-shaped source exists so the S1
`source_queue` pointer continues to identify the actual store. Do not use
`pqParent` for logical ownership: it has DA mutex, shutdown and destruction
semantics. Keep `pqParent == NULL`, `pqDA == NULL` and DA flags disabled.

Use a dedicated internal FE initializer/destructor. It may factor common object
initialization from `qqueueConstruct`, but must not call ordinary `qqueueStart`
and allocate an unused FixedArray, install global admission callbacks or create
misleading standalone logical statistics. With the private role pointer, the
queue type can remain the validated memory type; no new public queue type is
needed. All storage, lifecycle and destruction entries must dispatch on the
private role before interpreting that type as an ordinary FixedArray.

An explicit FE destruction branch is required: existing `qqueueDestruct` can
destroy `pWtpReg` before storage cleanup. That order is invalid while an FE lease
still lives in the worker. Retain the worker object until maintenance has
reconciled its lease, even after the thread has joined. Track partially
initialized resources so allocation/start failure cleanup touches only initialized
mutexes, conditions, worker slots and arrays.

### 2.1 Concrete existing API sequence

The following is a call-order sketch, not compilable implementation. The setter
names already exist in [wtp.h](../../../runtime/wtp.h); `ConsumerFE`,
`CompleteFELease` and the FE-specific callbacks are proposed. Supply correctly
typed callback wrappers rather than relying on unchecked function-pointer casts.

```text
wtpConstruct(&fe.pool)
wtpSetDbgHdr(fe.pool, stable_debug_name, name_length)
wtpSetpUsr(fe.pool, fe.source_queue)
wtpSetpmutUsr(fe.pool, &fe.mutex)
wtpSetiNumWorkerThreads(fe.pool, 1)
wtpSettoWrkShutdown(fe.pool, -1)
wtpSetbAllowFirstWorkerToTimeout(fe.pool, 0)
wtpSetpfChkStopWrkr(fe.pool, ChkStopFE)
wtpSetpfGetDeqBatchSize(fe.pool, GetFEBatchSize)
wtpSetpfDoWork(fe.pool, ConsumerFE)
wtpSetpfObjProcessed(fe.pool, CompleteFELease)
wtpConstructFinalize(fe.pool)
```

Leave `pfRateLimiter` and `pfIdleTimeout` unset: S2 rejects FE rate scheduling and
does not retire idle FE workers. `GetFEBatchSize` returns
`D = min(logical dequeue ceiling, F)`. Finalization allocates the worker batch,
deferred destructor buffer and private action-worker array. Preallocate this
bounded state as part of the FE reservation where practical; report allocation
bytes separately from the message-obligation bound `B + N*(F + D)`.

To activate a claimed registration, hold its FE mutex and call
`wtpAdviseMaxWorkers(fe.pool, 1, DENY_WORKER_START_DURING_SHUTDOWN)`. Check the return
and successful worker state before release-publishing a usable registration.
This call waits for the worker's signal-mask/cleanup initialization. The worker
cannot acquire the FE mutex until the caller releases it. Registration/shutdown
serialization must cover this whole publication decision; a pool state check
alone is not the family routing-quiescence protocol.

Publish no FE messages on start failure. Route the still-owned producer batch to
BE with the approved local wrapper reference semantics. Do not repeatedly reuse
or restart a failed registration behind an existing producer cache entry.

### 2.2 Lease and mutex identities

For every FE callback and completion, retain:

```text
worker.source_queue == worker.pWtp.pUsr == fe.source_queue
worker.logical_owner == fe.logicalOwner
worker.pWtp.pmutUsr == fe.source_queue.mut == &fe.mutex
```

Install the physical source's immediate-shutdown pointer before executing the
logical owner's callback. This supersedes the original logical-owner-pointer
proposal: an FE must stop cooperatively while BE remains available for transfer. Do not swap `pmutUsr` to the BE mutex. The callback uses this worker's
own `actWrkrInfo`, transaction parameters, execution state and deferred buffer.
The FE descriptor and logical owner outlive the thread, active lease and cached
producer reference.

## 3. Ring primitive and publication protocol

Implement a standalone bounded SPSC primitive, separate from worker and message
policy. Proposed operations are initialization over preallocated storage,
`try_push_batch(messages, actual_count)`, `pop_batch(destination, maximum_count)`,
a queued snapshot and destruction after quiescence. A failed push consumes no
references. A successful push transfers the entire actual batch. Pop transfers
the actual returned pointers into the worker's only lease; no extra references
are needed merely to move between these owners.

Separate producer and consumer index/cache state onto different cache lines.
Initialize slots before release-publishing the write sequence. Acquire that
sequence before the consumer reads slots. Copy pointers into the lease before
release-publishing the read sequence; only then may the producer reuse slots.
Cached opposite indices may prove a conservative fit. A cached no-fit result
must acquire-refresh the read sequence before routing to BE. Consumer empty
checks likewise refresh published availability before parking.

Use unsigned sequence arithmetic with usable capacity below half the sequence
range and validate allocation/count conversions. Explain the wrap proof. F=10,000
is not a power of two: `sequence % F` selects the wrong next slot when an unsigned
sequence wraps. Prefer independent producer/consumer slot cursors bounded by F,
advanced with checked wrap and split copies. Alternatively, allocate a rounded
power-of-two slot array, keep usable capacity F distinct and report its larger
allocation. Do not silently round the operator's usable capacity.

Atomic availability is not automatically lock-free on every supported target.
Check supported widths and provide the documented fallback or fail local-scope
activation on a target lacking the qualified implementation. Test non-power-of-two
capacity, exact fit, oversized batches and sequence/cursor wrap separately.

## 4. Initial FE-local wake protocol

After release-publishing a batch, the producer unconditionally locks the FE mutex,
signals the one worker's `pcondBusy`, then unlocks. No BE mutex is acquired on a
successful FE route. There is no producer wait for FE space; a no-fit decision
selects BE and its established admission policy.

The consumer holds that same FE mutex while checking its lease, control and ring
predicates. Existing `wti` idle processing atomically releases it when waiting.
Publication before the consumer's empty check is observed by the acquire read;
publication after that check must complete its mutex-protected signal after the
consumer releases the mutex to wait. Spurious signals simply cause a recheck.
The producer may wait briefly for the FE control mutex; callbacks and final
message destruction must run outside it.

Do not optimize this into an unlocked `if (waiting)` check. Existing
`doIdleProcessing` registers the waiter without a post-registration ring recheck.
A new armed flag would need its own complete memory-order/wakeup proof. Keep
this initial protocol measurable and simple; no spin loop, futex or eventfd is
needed for S2. The rejected shared-queue waiter-targeting experiment in the
[rejected paths catalog](../rejected_engineering_paths.md) does not justify
reviving that policy as part of FE notifications.

## 5. FE acquisition, completion and retained retry

Dispatch FE work before the ordinary queue bookkeeping. Merely assigning the
existing `qDeqBatch`/`qCompleteBatch` hooks is insufficient: their callers assume
segmented store contexts, physical retirement counters and whole-batch deferral.
Prefer the dedicated pool callbacks plus a source-role branch in the generic S1
completion dispatcher. Global stores retain their existing path.

`ConsumerFE` enters and returns with the FE mutex held. Before acquiring a batch,
settle the preceding lease or retain it. Bind the source before any acquisition
can leave responsibility behind. Pop at most D entries, initialize actual batch
counts and element states, then release ring capacity. Unlock the FE mutex,
drain safe deferred references, and enable cancellation only around the qualified
consumer callback. Disable cancellation again before relocking. Always install
the logical immediate-stop pointer before cancellation can deliver inside the
callback.

`CompleteFELease` classifies actual element outcomes and releases only proven
terminal responsibility. RDY/SUB entries remain in the worker's bounded lease;
the consumer never republishes them to the SPSC. A retained lease forbids another
acquisition even when the ring is empty. It also forbids freeing transaction or
parameter state needed for retry. Completion is idempotent across normal exit,
cancellation and joined-worker maintenance.

### 5.1 Retry and partial-batch caveats

At the audited baseline, `ruleset.processBatch` runs every element without
skipping COMM entries. Replaying an unchanged partial batch therefore repeats
already completed scripts. Once callback transactions are reusable, compact only
unresolved entries before a new execution attempt. Until then, retain the
original array and module state without compaction or overwrite. The callback
qualification must establish when transaction state is reusable; checking only
ring emptiness or batch count cannot establish it.

Existing qualified action retry paths should normally wait within their callback.
If a returned unresolved lease has an explicit retry-ready condition or deadline,
the FE adapter must wait on that condition and shutdown under its mutex, using a
monotonic timed condition where needed. FE publication can wake the wait but does
not make a retry due. Do not return repeated completion errors into `wtiWorker`
and spin: the generic loop has no error backoff. An unexpected unresolved return
without an approved resume contract remains retained, with a bounded diagnostic,
until shutdown; future submissions may overflow to BE.

`actionCommitAllDirect` currently discards `actionCommit`'s result, while ruleset
elements may already be COMM. These notes do not repair that behavior or infer
output delivery from it. Omfile error/cancellation qualification must distinguish
script completion, output delivery and deliberate terminal loss. If the required
outcomes cannot be established, keep that callback configuration disabled until
the owning stage supplies the necessary behavior and tests.

## 6. Normal exit and cancellation cleanup

Normal `wtiWorker` exit marks the worker exiting, unlocks `pmutUsr`, removes its
private module instances from action worker tables, frees those instances and
releases cached transaction/nontransaction parameters. Its cancellation handler
instead completes the batch under `pmutUsr`; cancellation unwinds past the normal
action-cleanup block. The outer `wtp` cleanup marks WAIT_JOIN and decrements the
active-thread count. The later action destructor can free instances still in
its table, but that is not an FE-local proof that all parameter references have
been reconciled before message transfer.

For S2, extract the normal action-state disposal into an idempotent helper and
invoke it on the local FE cancellation exit as well. Keep cancellation disabled;
settle/tag the FE lease under its mutex, release that mutex, and dispose of
worker-private action state. Retain any message references that those parameters
could still use until disposal completes. Remove the instance from the action's
worker table exactly once so later action destruction cannot free it again.
Keep the global cancellation behavior outside this scoped change unless separately
reviewed. Constructor failure cleanup is a different path and needs no running
module callback.

Only after thread join may maintenance inspect the retained batch and assume no
callback or module destructor can still access it. Join is not lease completion:
the `wti` object, its batch and source identity remain live until each obligation
is transferred or explicitly discarded. Deferred destruction must likewise be
empty before `wtiDestruct`. If a callback cannot be cancelled safely, retaining
live state is mandatory; timeout expiry does not permit freeing a running thread.

## 7. Counters and internal accepted-obligation transfer

Keep reference storage, queue capacity and delivery responsibility distinct.
BE FixedArray physical capacity already includes its in-flight entries until
retirement; FE ring slots become reusable after dequeue, so bounded FE active
holdings are additional capacity.

| Event | Responsibility change |
|---|---|
| External FE publication of n | One external attempt/admission per message; FE queued gains n |
| FE dequeue of n | Queued moves to active; outstanding unchanged |
| Retained retry of n | Active moves to retry, never both |
| Proven terminal completion of n | Active/retry moves to the defined terminal outcome |
| FE-to-BE transfer of n | FE holding moves to BE inventory; no external attempt/admission |
| Shutdown policy discards n | Holding moves to explicit terminal shutdown discard |

Ring sequences describe queued entries. Active/retry gauges describe disjoint
unresolved obligations. When transaction lifetime requires retaining references
to already terminal elements, report that storage separately instead of counting
those obligations twice. Use producer-owned admission counters and consumer-owned
completion counters with atomic snapshots; exact reconciliation occurs at
quiescent barriers. Historical `enqueued` remains arrival attempts.

Proposed internal API, distinct from external enqueue:

```text
transferAcceptedToBE(logical_owner, owned_message, monotonic_deadline)
    -> TRANSFERRED | RETAINED | TERMINALLY_DISCARDED
```

TRANSFERRED means the BE owns the supplied reference and has updated its existing
physical accounting and worker wakeup state. RETAINED leaves the same reference
with the FE lease/maintenance owner. TERMINALLY_DISCARDED is allowed only under
the explicit memory shutdown policy, consumes the reference and records its
terminal outcome. No result creates another external attempt or admission.

Under the BE mutex, check capacity and perform the nonallocating FixedArray add,
physical/overall accounting and ordinary BE worker advice. A full BE may wait
only within the remaining logical deadline while BE consumers can progress;
otherwise return RETAINED. Do not call external `doEnqSingleObj` blindly: it
counts arrivals and can destroy messages on timeout. Do not use MsgAddRef to
conceal uncertainty about which owner received the obligation.

Transfer a bounded prefix and preserve the exact unprocessed suffix. Debit the
FE obligation only after the receiving outcome is known. A batch may contain
transferred, terminal and retained entries; a failed suffix must never retry the
accepted prefix. Maintenance can reuse the joined worker's bounded batch to pop
ring entries after its earlier lease is settled. No second transfer buffer or
second concurrent FE consumer is required.

## 8. Family shutdown and monotonic deadlines

Publish REDIRECT, close registration and establish that all pre-transition FE
publication sections have completed. Routing quiescence is a prerequisite; neither
an empty ring nor a producer's earlier RUNNING read proves it. Internal/unclassified
submissions remain BE-only, with explicit final admission closure before resource
destruction. Do not hold the registry mutex across callback joins or BE waits.

Request every FE to stop acquiring after its current callback. Let active
callbacks finish within the one logical graceful deadline. Pool immediate-stop
state can stop the next acquisition while the physical source's immediate flag
remains unset; these are distinct controls. Complete or retain the active lease,
join the FE thread, and let maintenance transfer residual work to the still-running
BE. BE must not terminate merely because it is temporarily empty before transfers
finish. After all accepted upstream/FE obligations are settled, request BE drain
and wait using the remainder of the same deadline.

At graceful deadline expiry with unfinished FE obligations, set each running
FE physical source's immediate flag and request its stop transition. Preserve BE
service while FE callbacks finish and maintenance transfers residuals. Use one
new action-completion deadline for the family. Request cancellation on all
unfinished FEs before joining any of them. After FE transfer attempts, close BE
admission, settle existing submissions, and request BE drain. BE escalation uses
the remaining action deadline, or starts that single deadline if no FE needed
it. This supersedes the original simultaneous FE/BE immediate-stop proposal.

If BE cannot consume further transfers, retain the exact residual ownership
until explicit terminal memory-shutdown discard. Do not restart BE implicitly
or multiply a full timeout by N. Cancellation/join may exceed the policy
deadline while safe thread termination completes; do not claim a hard wall-time
bound for an uncooperative callback.

### 8.1 Required worker-pool wait seam

Current `wtpShutdownAll` combines request and wait. Its `condThrdTrm` uses the
default realtime clock, and existing `timeoutComp` supplies realtime deadlines.
Add separate request-all and wait-until operations for the local family. Retain
the ordinary global wrapper's behavior. Each local wait consumes the same
absolute monotonic phase deadline, not a newly calculated full interval.

Choose and test a real monotonic condition-wait mechanism: an available
`pthread_cond_clockwait`, or a monotonic condition initialized for local FE and
BE pools before any waiters exist. The latter needs an explicit clock selection
in construction; do not pass monotonic timestamps to a realtime condition or
reinitialize a live condition. Retry waits and internal BE capacity-transfer
waits need the same clock discipline. Converting a deadline to realtime once
does not protect a long wait against a later backward clock adjustment.

Preserve the existing lock order: FE/BE `pmutUsr` before pool-management mutex
where required, without holding a different source's mutex. Request state and
signal under the pool's own predicate mutex; release it before awaiting another
pool. Every retained source, logical owner, worker and stats record survives
until the family reaches its final reconciled state.

## 9. Patch boundaries and implementation evidence

| Patch boundary | Principal files | Acceptance evidence required later |
|---|---|---|
| Standalone SPSC primitive | New `runtime` primitive; runtime manifest; tests registered in `tests/Makefile.am` | Exact/no fit, cached-full refresh, partial reads, wrap, publication ordering |
| FE source and one-worker adapter | `runtime/queue.[ch]`, narrow worker hooks | Private WID/source identity, start failure, registration cap, producer departure, empty-to-park wake |
| Lease completion/retry and cancellation | `runtime/queue.c`, `runtime/wti.[ch]` | Partial terminal states, no spontaneous replay during an ordinary retained lease, deliberate ambiguous shutdown replay, retained retry without spin, cancellation/action cleanup |
| Internal transfer and family deadline | `runtime/queue.c`, `runtime/wtp.[ch]` | Full BE, partial transfer, callback cancellation, late internal messages, N-front shared deadlines |

Registration/configuration and stats registration remain separately assigned
integration work. Core stats movements belong with the ownership code that
performs them. Do not edit another agent's `queue.c`, worker files or test
manifest concurrently. The stage's full required container, sanitizer,
portability, security/concurrency review and performance gates remain in the
implementation plan; focused primitive tests alone do not accept S2.

## 10. Source evidence and note validation

Source links below identify files; symbol names avoid relying on line numbers
that will shift during S1 integration.

| Source | Relevant symbols and observed assumptions |
|---|---|
| [queue.c](../../../runtime/queue.c) | `qqueueConstruct`, `qqueueStart`, `qqueueDestruct`: common initialization, storage/pool setup and early pool destruction |
| [queue.c](../../../runtime/queue.c) | `ConsumerReg`, `DeleteProcessedBatch`, `DequeueConsumableElements`, `batchProcessed`: mutex/cancellation discipline, source-specific retirement and RAM re-enqueue |
| [queue.c](../../../runtime/queue.c) | `doEnqSingleObj`, `qAddFixedArray`, `qqueueShutdownWorkers`: admission/reference outcomes, storage insertion and coupled shutdown |
| [wti.c](../../../runtime/wti.c) | `wtiConstructFinalize`, `wtiWorker`, `wtiWorkerCancelCleanup`, `doIdleProcessing`: private allocation, action cleanup asymmetry and condition waiting |
| [wtp.c](../../../runtime/wtp.c) | `wtpConstructFinalize`, `wtpStartWrkr`, `wtpAdviseMaxWorkers`, `wtpShutdownAll`, `wtpWorker`: allocation, startup acknowledgement, request/wait coupling and joins |
| [ruleset.c](../../../runtime/ruleset.c) | `processBatch`: every batch element executes; Direct transactions commit afterwards |
| [action.c](../../../runtime/action.c) | `actionCommit`, `actionCommitAllDirect`, `actionRemoveWorker`, `freeWrkrDataTable`: retry state, discarded return and module-instance ownership |

This note was prepared during active baseline measurements. Validation is limited
to source/text review, local link/metadata checks, `git diff --check`, and the
internal-document-only classification from `devtools/local-validation-plan.sh`.
No build, runtime test, container run, sanitizer or timing was performed for
this document. No runtime implementation or PR-readiness claim follows from it.
