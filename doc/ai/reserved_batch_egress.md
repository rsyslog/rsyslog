# Experimental reserved-batch ruleset egress

`global(executionEngine="reservedBatch")` selects an experimental execution
adapter. The default, `legacy`, does not enter this code. Phase 1 handles only
asynchronous `call` and `call_indirect` targets backed by bounded, memory-only
LinkedList or FixedArray ruleset queues. Direct calls, action queues, plugin
callbacks, messages, and disk formats are unchanged.
An explicitly configured `queue.type="Direct"` ruleset is not an asynchronous
target: `rulesetHasQueue()` remains false for it, so calls execute synchronously
and may mutate the current message exactly like queue-less ruleset calls.

## Ownership and state

A WTI ledger exists only for one source dequeue batch. Its states are
`EMPTY`, `EXECUTING`, `PUBLISHING`, and `PUBLISHED`. Each accepted branch owns
one `MsgDup()` result, one target capacity reservation, and (for LinkedList) a
prepared node. Cancellation is disabled from reservation through installation
in the ledger. Publication transfers all three exactly once; cancellation
cleanup publishes accepted branches before normal source-batch cleanup.

The target invariant, under its queue mutex, is:

```
physical queue entries + unpublished reservations <= queue.size
```

Publication is a bounded cancellation-disabled section. Each target bucket is
published with one lock acquisition, accounting/persistence update, and worker
advice. Prepared publication has no allocation or capacity failure path.
If a new reservation encounters exhausted physical-plus-reserved capacity, a
slow path first publishes every bucket currently staged by this WTI and then
retries the ordinary wait/timeout/drop admission loop. Buckets are visited in
their encounter order, and one target lock is released before the next is
acquired. Flushing all buckets prevents crossed-target reservation cycles
between WTIs while retaining end-of-source-batch publication on the normal
path.
Encounter order from one WTI to one target is retained. Concurrent WTIs are
ordered by target publication order, not by global call encounter time.

## Source and action relation

The WTI captures the config pointer and selector at source-batch entry. Every
call in that batch uses the captured selector; no statement rereads `runConf`.
The current daemon does not swap the active main configuration during live
processing (HUP deliberately does not reload it), so the source queue worker
lifetime also covers the captured config and target queues.

Source `BATCH_STATE_*` transitions remain legacy: a successful `scriptExec()`
marks its source element committed; an interrupted element remains ready for
retry. A prepare/allocation failure is returned through the internal main-queue
consumer adapter so its source state is not overwritten by the historical
blanket commit. Reserved branches publish before `actionCommitAllDirect()`,
matching the legacy relation in which queued calls have already submitted
before the end-of-batch direct transaction commit. The action state machine,
plugin callbacks, and action transaction cleanup are not adapted.

Legacy does not guarantee exactly-once across hard cancellation: a queued call
may submit before its source message is interrupted and retried. Reserved-batch
cleanup deliberately preserves that behavior by publishing every accepted
branch, including branches encountered by an incomplete source element.
The same rule applies when a later branch fails to allocate: an earlier accepted
branch is published and normal source retry may publish it again, just as the
legacy engine may retry a source after its earlier `submitMsg2()` already
succeeded. Filtering publication by source commit state would silently strengthen
legacy delivery semantics and could lose an accepted branch if termination wins
before the source retry. The focused late-failure test therefore asserts the
permitted duplicate and, critically, the absence of branch loss.

Each branch ledger record transitions from `STAGED` to `PUBLISHED`; published
records have no message or prepared-node ownership. Slow-path publication
updates that state before cancellation is restored, so cleanup publishes only
the remaining staged suffix. No source index is retained because Phase 1 never
filters publication by source commit state.

Admission counters retain legacy meanings. `enqueued` and inbound bytes are
counted once when a branch first attempts admission. A pressure probe used to
trigger the slow path does not increment `full`; the subsequent ordinary
admission loop increments `full` for each full observation and increments the
full-discard counter only on the same immediate/timeout rejection paths as
legacy.

When configured with the default-off `--enable-reserved-egress-stats`, queue
impstats expose `egress.published.batches`,
`egress.published.messages`, and `egress.publication.advice`. They are regular
stats counters updated once under the existing target publication lock. The
fields, registration, and updates are compiled out unless this dedicated
diagnostic option is enabled, so ordinary legacy and reserved-batch timing
builds add no shared diagnostic RMW. On the no-pressure path, one populated
target bucket for a source dequeue batch increments batches and advice once
regardless of its branch count.

## Internal test hooks

`--enable-reserved-egress-test-hooks` is a default-off configure option for
deterministic selector-capture and allocation-failure tests. It is independent
of imdiag on purpose: ordinary imdiag and benchmark builds compile neither the
environment probes nor their shared hook state. A worker-advice gate lets tests
form a source dequeue batch from already available messages without enabling
`queue.minDequeueBatchSize`; removing the gate and submitting one release message
returns immediately to ordinary advice. The hook-dependent tests are registered
only when both imdiag and this option are enabled.
The publication-counter oracle additionally requires
`--enable-reserved-egress-stats` and impstats. A separate non-hook test submits
a quiet-queue singleton, a burst, and a final singleton tail, observing each
before any later arrival; this proves dequeue and publication batch sizes are
ceilings rather than fill requirements.

## Exclusions

Disk, segmentedDisk, disk-assisted or unbounded queued rulesets, sampling
queues, Direct source queues, persistent continuations, producer lanes, SCQ,
and wCQ are outside this phase. Queue-less ruleset calls remain synchronous on
the legacy path. Configuration activation rejects unsupported queues rather
than silently mixing execution engines.
