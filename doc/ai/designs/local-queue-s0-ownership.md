<!--
.. meta::
   :description: S0 source inventory of rsyslog queue admission, batch ownership, locking, completion, cancellation, and shutdown contracts.
   :keywords: rsyslog, queue, S0, ownership, batch, locking, shutdown, disk assistance, retry
-->

# Local queue S0: ownership and lifecycle inventory

| Metadata | Value |
|---|---|
| Status | **S0 source audit; no runtime implementation** |
| Created / reviewed | 2026-09-14 |
| Design authority | [Local queue front ends](local-queue-frontends.md) and [implementation plan](local-queue-implementation-plan.md) |
| Documentation baseline | `8debb0a697158b7ccd2c243a2262dc2537b7c64a` |
| Runtime source baseline | `b0d9f971f007f06db3f734543cef5dfb312c5090` |
| Scope | Existing global queue behavior and the minimum S1 ownership seam |
| Non-scope | Runtime behavior, configuration, queue storage formats, measurements, and test execution |

<!-- .. summary-start -->
The current worker owns one reusable batch, while its worker pool identifies one
queue as both callback owner and completion source. S1 can preserve global
behavior by adding private batch-lease metadata to the worker, setting its source
at acquisition, and completing through that source on normal and cancellation
paths. Enqueue outcomes consume message references differently by path; local
routing must preserve those distinctions.
<!-- .. summary-end -->

This is a source inventory for the staged local-queue design. It describes the
observed baseline, not an implemented local-scope feature. Proposed S1 seams are
explicitly labeled and do not change the S2 support decision, option matrix, or
activation whitelist.

## 1. Current ownership model

One `qqueue_t` holds storage, counters, queue conditions, an immediate-shutdown
flag, and its regular worker pool. A disk-assisted (DA) memory parent creates a
disk child and a separate one-worker transfer pool. The child shares the parent's
queue mutex, but owns its own storage state and its own immediate-shutdown flag.
See [`qqueue_t`](../../../runtime/queue.h#L94-L270),
[`StartDA`](../../../runtime/queue.c#L785-L900), and
[`InitDA`](../../../runtime/queue.c#L909-L945).

`wti_t` owns a reusable `batch_t`, deferred message-destructor buffer, private
action-worker state, and a pointer to its worker pool. The batch is not a lease:
it has message elements, per-element states, dequeue count, dequeue identifier,
and optional store context, but no source queue. See
[`wti_s`](../../../runtime/wti.h#L73-L115) and
[`batch_t`](../../../runtime/batch.h#L61-L106).

The regular pool binds `pUsr`, `pmutUsr`, `pfDoWork`, and `pfObjProcessed` to the
same queue. Consequently, `batchProcessed(pUsr, pWti)` currently completes the
worker's current batch against the callback queue. This is correct only while a
worker never obtains work from another store. See
[`qqueueStart`](../../../runtime/queue.c#L3898-L4104) and
[`wtp_s`](../../../runtime/wtp.h#L47-L78).

## 2. Submission and message-reference inventory

| Route | Existing path | Observed ownership / outcome |
|---|---|---|
| Single input submission | [`submitMsg2`](../../../tools/rsyslogd.c#L1282-L1297) selects the main or ruleset queue and calls [`qqueueEnqMsg`](../../../runtime/queue.c#L4761-L4788). | `submitMsg2` does not inspect the enqueue return. A successful memory admission transfers the supplied reference to the queue; an error can leave it with the caller, depending on the storage path below. |
| Multi-input submission | [`multiSubmitMsg2`](../../../tools/rsyslogd.c#L1304-L1327) calls `pQueue->MultiEnq` and then clears `nElem`. | [`qqueueMultiEnqObjNonDirect`](../../../runtime/queue.c#L4711-L4737) handles entries individually under one queue mutex. `RS_RET_QUEUE_FULL` is accepted for that entry and processing continues; another error ends the loop. This is not an atomic all-or-none admission contract. |
| Queued action fan-out | [`doSubmitToActionQ`](../../../runtime/action.c#L2189-L2220) adds a message reference or duplicate, then calls `qqueueEnqMsg`. | The action queue receives a distinct delivery reference. Even while an upstream worker has a larger batch, this route submits one message at its mutation-sensitive execution point. |
| DA transfer | [`ConsumerDA`](../../../runtime/queue.c#L3762-L3844) gives the child `MsgAddRef(message)`. | The parent entry is marked committed only after the child path accepts it. A segmented-child failure leaves the current and following parent entries retryable. |

`doEnqSingleObj` accounts arrival first, then applies discard, flow control, and
the storage add. It requires the source queue mutex. See
[`doEnqSingleObj`](../../../runtime/queue.c#L4539-L4697). A local wrapper must
retain this distinction between logical admission and the final physical store;
it cannot infer ownership only from a successful-looking batch call.

### 2.1 Failed enqueue outcomes are storage-specific

The baseline does **not** have one universal rule that a failed `qAdd` consumes
the message reference.

| Outcome | Reference consumption in the observed path |
|---|---|
| Sampling drop in [`qqueueAdd`](../../../runtime/queue.c#L2334-L2373) | Consumed: `msgDestruct()` runs and the call returns success. |
| Discard watermark in [`qqueueChkDiscardMsg`](../../../runtime/queue.c#L2792-L2830) | Consumed: it destroys the message and returns `RS_RET_QUEUE_FULL`. |
| Full queue with immediate discard, enqueue timeout, or condition error | Consumed: `doEnqSingleObj` destroys the message before returning `RS_RET_QUEUE_FULL`. |
| Full queue interrupted by global input termination | Not consumed in this function: it returns `RS_RET_FORCE_TERM` before destruction. |
| FixedArray add | Retained by the queue after the slot write; this handler has no failure return. See [`qAddFixedArray`](../../../runtime/queue.c#L998-L1007). |
| LinkedList allocation failure | Not consumed by `qAddLinkedList`; allocation fails before the message is placed in a node. See [`qAddLinkedList`](../../../runtime/queue.c#L1065-L1087). |
| Classic disk write failure before successful serialization/flush | Not generally consumed by `qAddDisk`; destruction occurs only after successful write and flush. See [`qAddDisk`](../../../runtime/queue.c#L2128-L2172). |
| Segmented-disk append or marker failure | Consumed: `qAddSegDisk` destroys the queue-owned reference in its finalizer even on error. See [`qAddSegDisk`](../../../runtime/queue.c#L1256-L1285). |
| Direct callback | Consumed after invoking the consumer, including if that callback reports an error. See [`qAddDirectWithWti`](../../../runtime/queue.c#L2282-L2308). |

S2 must specify reference consumption for every FE publish, no-fit fallback,
registration failure, redirect, and shutdown result. It must also preserve the
existing multi-submit behavior: whole-tier selection may route a producer batch
as a unit, but BE admission remains per-entry unless a separately justified
contract changes it.

## 3. Batch lifecycle and source-dependent completion

The ordinary regular-consumer sequence is:

1. [`wtiWorker`](../../../runtime/wti.c#L438-L561) holds `pWtp->pmutUsr` and
   calls `pfDoWork`.
2. [`ConsumerReg`](../../../runtime/queue.c#L3686-L3750) dequeues under that
   mutex, unlocks it for the consumer callback, then locks it again.
3. The next dequeue or [`batchProcessed`](../../../runtime/queue.c#L3661-L3679)
   invokes [`DeleteProcessedBatch`](../../../runtime/queue.c#L3099-L3160).
4. Completion re-enqueues `RDY` and `SUB` RAM entries to the same source,
   retires the source store, moves message references to the worker's deferred
   buffer, and destroys them outside the queue mutex.

For classic RAM stores, retirement uses `deqID` and the to-delete list so a later
completed FixedArray batch can release capacity without waiting for an older
batch. [`DeleteBatchFromQStore`](../../../runtime/queue.c#L2992-L3057) documents
that contract. For segmented disk, `batch.storeData` identifies a pending store
context; completion re-appends retryable entries, advances the store in order,
and releases that context only after the store accepts it. See
[`segdiskStoreCompleteBatch`](../../../runtime/segdisk_store.c#L1468-L1498).

The consumer callback itself changes only element states. Ruleset processing
marks a message committed after successful script execution; action processing
can retain ready state when a transactional action is interrupted during retry.
See [`processBatch`](../../../runtime/ruleset.c#L630-L667) and
[`processBatchMain`](../../../runtime/action.c#L2025-L2085). Completion therefore
must use actual `nElem`, `nElemDeq`, `eltState`, and `storeData`; `maxElem` is
allocated capacity, not an ownership or retirement count.

## 4. Lock, atomic, and lifecycle table

| State or operation | Owner / guard | Required relationship |
|---|---|---|
| Queue storage, admission, dequeue, retirement, `notFull`, flow-control conditions | `qqueue_t::mut` | Held by enqueue, dequeue, completion, and queue wait predicates. DA parent and child intentionally share the parent's mutex. |
| Worker wait predicates and `pcondBusy` | `wtp_t::pmutUsr`, set to the queue mutex | A producer reserves/signals a waiter while this mutex is held. [`wtiWaitNonEmpty`](../../../runtime/wti.c#L374-L397) and [`wtpAdviseMaxWorkers`](../../../runtime/wtp.c#L562-L617) depend on this pairing. |
| Worker start/join table | `wtp_t::mutWtp` plus atomic worker state | A worker slot remains owned until joined. [`wtpStartWrkr`](../../../runtime/wtp.c#L460-L549) and cancellation coordinate this state. |
| Pool shutdown state | `mutWtpState` atomic helper | A start checks it while holding `mutWtp`; normal new work is denied after shutdown starts. |
| Queue immediate-stop state | `bShutdownImmediate` atomic helper | Before a callback, [`qqueueSetWtiShutdownImmediate`](../../../runtime/queue.c#L662-L677) makes `pWti` read the source queue's flag. |
| Cancellation cleanup | Queue mutex via `pWtp->pmutUsr`, cancellation disabled by cleanup entry | [`wtiWorkerCancelCleanup`](../../../runtime/wti.c#L333-L361) marks the worker exiting, completes the active batch, and returns with no live lease. |
| Deferred destruction | Worker-private `p_deferred_msgs` | The queue mutex protects the move into the buffer; only that worker destroys entries after releasing it. [`qqueueDrainDeferred`](../../../runtime/queue.c#L3060-L3092). |

The worker framework assumes `pfDoWork` temporarily unlocks and then re-locks
the pool's `pmutUsr`. A future FE worker borrowing BE work cannot replace that
pointer or unlock an arbitrary source mutex underneath the framework. S1 must
therefore retain the stronger rule `source_queue == pWtp->pUsr` and
`source_queue->mut == pWtp->pmutUsr`; shared mutex equality alone is not
borrowing authority. Borrowing remains S3 work, after an explicit worker-loop
transition is designed and tested.

## 5. Shutdown, cancellation, and DA ordering

`qqueueShutdownWorkers` applies one coordinated sequence to a top-level queue:
regular shutdown first, action-timeout shutdown when parent or child work remains,
then cancellation and joins. It requires a parent queue and treats the DA child
as coupled state. See [`qqueueShutdownWorkers`](../../../runtime/queue.c#L2604-L2657),
[`tryShutdownWorkersWithinQueueTimeout`](../../../runtime/queue.c#L2386-L2450),
and [`cancelWorkers`](../../../runtime/queue.c#L2536-L2601).

Immediate shutdown sets both parent and DA-child flags. Save-on-shutdown runs
only after regular workers are stopped and can restart the DA transfer pool to
persist residual parent memory work. See
[`tryShutdownWorkersWithinActionTimeout`](../../../runtime/queue.c#L2461-L2533)
and [`DoSaveOnShutdown`](../../../runtime/queue.c#L4326-L4369). Local scope must
add producer-routing quiescence and FE-to-BE ownership transfer before this
existing ordering may inspect queues or restart DA transfer.

## 6. Proposed S1 batch-lease seam

**Proposed, internal-only:** add two nullable pointers to `wti_t`,
`source_queue` and `logical_owner`. Do not add a speculative lease enum, modify
`batch_t`, or alter the public consumer callback. A NULL `source_queue` means
there is no active responsibility. Existing `batch.storeData` remains owned by
its storage backend.

Before a new acquisition, complete the preceding active source or retain it on
completion failure; never overwrite its pointers. Bind both pointers before any
dequeue path can return idle, error, or a discard-only result. This includes a
batch with `nElem == 0` but `nElemDeq > 0`, and a store context returned with an
error. The source remains live until actual responsibility has been retired or
transferred: a `qCompleteBatch` failure retains it, while normal RAM retirement,
segmented-store completion, or a deliberate DA transfer may clear it only after
their existing ownership actions have succeeded.

For global mode, `source_queue == pWtp->pUsr == pThis`; callback behavior,
queues, mutexes, and storage execution therefore remain unchanged. For a DA
child, `logical_owner` is `pqParent ?: pThis`, while `source_queue` is the child
that the regular child pool services. The source must equal the pool-serviced
queue and use its `pmutUsr`; S1 rejects a source with a different mutex instead
of attempting nested or substituted locking. Install the source queue's
immediate-stop pointer before the callback. Normal completion and
`wtiWorkerCancelCleanup` dispatch to `source_queue`.

This makes wrong-source completion detectable without granting borrowing. A
narrow dispatch test can acquire from source A and invoke the generic completion
callback associated with source B; only A may change records, retry state,
counters, or wake a producer. A separate different-mutex test must be rejected.
Native DA parent/child tests prove the physical/logical split without relaxing
the source-to-pool relationship. Preserve the inactive-batch completion path,
because worker shutdown can call `pfObjProcessed` when no batch was acquired.

## 7. S1 and S2 test mapping

| Contract | Existing evidence to retain | New or later evidence |
|---|---|---|
| Out-of-order RAM completion | [`queue-fixedarray-outoforder.sh`](../../../tests/queue-fixedarray-outoforder.sh) | Narrow dispatch test must prove completion affects the acquired store only. |
| Deferred cleanup and missed-wakeup boundaries | [`queue-deferred-idle-wakeup.sh`](../../../tests/queue-deferred-idle-wakeup.sh), [`queue-deferred-idle-shutdown.sh`](../../../tests/queue-deferred-idle-shutdown.sh) | Cancellation with an active lease, including deferred destructor ownership. |
| Worker wait and lifecycle | [`wtp-targeted-wakeup.sh`](../../../tests/wtp-targeted-wakeup.sh) | S2 FE/BE wake protocol and producer-capacity wakeup tests. |
| Transactional partial outcomes | [`action-tx-errfile.sh`](../../../tests/action-tx-errfile.sh) | Normal, retry, partial, and cancellation outcomes in one deterministic lease oracle. |
| DA save and restart | [`classic-da-persist.sh`](../../../tests/classic-da-persist.sh), [`segmented-da-persist.sh`](../../../tests/segmented-da-persist.sh) | S1 confirms native logical-parent/physical-child ownership; S5 adds FE drain before DA save/restart. |
| DA retry and interrupted shutdown | [`segmented-da-retry-restart.sh`](../../../tests/segmented-da-retry-restart.sh), [`wti-shutdown-saveonshutdown-omhttp.sh`](../../../tests/wti-shutdown-saveonshutdown-omhttp.sh) | S5 source-correct transfer, repeated shutdown, and recovery without old FE identity. |

S1's new deterministic dispatch test must contain inline intent and oracle text
as required for testbench changes. It should distinguish stores with independent
completion counters or records, deliberately exercise the wrong callback owner,
reject a different-mutex source, and prove normal completion, retry, partial
states, and cancellation each retain one responsible source.
