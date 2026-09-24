# Rejected and parked engineering paths

catalog_updated: 2026-09-10
default_review_horizon_days: 180
default_binding_horizon_days: 540

This is the canonical log of approaches that were tried, measured or
reviewed, and then **not pursued**. It exists so agents and humans do not
reopen the same dead ends from first principles.

It is not a performance leaderboard and not a substitute for the
[`rsyslog-performance`](../../.agent/skills/rsyslog-performance/SKILL.md)
measurement contract. Component benchmark READMEs and tracked JSON remain
the detailed evidence. This file stores the **decision**: what, result,
why we stopped, and when that decision becomes stale.

## How to read dates

Every catalog and entry date is ISO-8601 (`YYYY-MM-DD`).

| Field | Meaning |
| --- | --- |
| `catalog_updated` | Last add, edit, or status change in this file |
| `recorded` | When the attempt was concluded |
| `last_reviewed` | Last time a human or agent confirmed the write-up still matches the evidence |
| `stale_after` | After this date the entry is **historical**, not binding |

Default horizons from `catalog_updated` / `recorded`:

- Re-read (`default_review_horizon_days`): 180 days
- Stop treating as current evidence (`default_binding_horizon_days`): 540 days

An entry is **stalled or too old** when any of these is true:

- today is after `stale_after`
- `last_reviewed` is more than `default_review_horizon_days` before today
- `catalog_updated` is more than `default_review_horizon_days` before today
  and no newer entry exists in the touched area
- the compared revisions, compiler, or host class no longer match current
  `main` in a way that could change the result

Stale does **not** mean "try it again immediately". It means: do not cite the
old ratio as current fact. Re-measure under the performance skill before
reviving the idea, and only if `reopen_if` is actually met.

## Status values

| Status | Meaning |
| --- | --- |
| `rejected` | Do not merge or retry the same design without new evidence |
| `parked` | Code or a PR may exist; not accepted as the next step |
| `measurement-rejected` | The *benchmark design* was invalid; the product idea is untested |
| `superseded` | A later retained change replaced this line of work |

## Entry template

Copy this block when adding a path. Keep IDs stable (`area-short-name`).
Do not store hostnames, home directories, or raw logs here.

```yaml
id:
status: rejected | parked | measurement-rejected | superseded
recorded: YYYY-MM-DD
last_reviewed: YYYY-MM-DD
stale_after: YYYY-MM-DD
area:
tried:
result:
why_stopped:
evidence:
reopen_if:
```

## Entries

### queue-waiter-targeting

```yaml
id: queue-waiter-targeting
status: rejected
recorded: 2026-09-05
last_reviewed: 2026-09-10
stale_after: 2028-02-27
area: runtime/queue.c wti/wtp wakeup
tried: >
  Direct idle-worker wakeup ("waiter targeting"), including a reservation-cap
  variant, on the FixedArray main-queue contention screen.
result: >
  Isolated multi-producer lifecycle screens were about 23% slower than the
  untargeted baseline (median ratios about 1.229 and 1.230, one session each).
why_stopped: >
  Throughput regressed. Extra wakeup policy complexity was not justified.
  Later retained work was deferred msgDestruct plus O(1) FixedArray retirement,
  not waiter targeting.
evidence:
  - benchmarks/queue-contention/README.md (Measured results, 2026-09-05)
reopen_if: >
  A new profile shows idle-worker selection as the dominant contended cost on
  current main, with a design that does not add lock hold or false wakeups,
  and two independent sessions beat the declared improvement target without
  a low-contention regression.
```

### queue-ll-enq-prealloc

```yaml
id: queue-ll-enq-prealloc
status: parked
recorded: 2026-09-10
last_reviewed: 2026-09-10
stale_after: 2028-03-03
area: runtime/queue.c LinkedList qqueueEnqMsg
tried: >
  malloc LinkedList enqueue cells before taking the queue mutex in
  qqueueEnqMsg only (not MultiEnq). Cells were handed to qAdd only after
  flow-control waits so a per-queue pEnqNode slot could not be overwritten.
  PR https://github.com/rsyslog/rsyslog/pull/7577
result: >
  On a shared WSL host (cpuset 0-15 of 28 CPUs), Direct main queue plus
  LinkedList action queue (1e6 msgs, 16 connections, 8+8 workers): two
  sessions, median after/before 1.022 and 1.034, MAD 0.034 and 0.029.
  Declared 5% improvement target missed; 5% regression guardrail held.
  Low-contention guardrail median 0.978. Review found two leak classes
  (publish-before-wait; abort before consume) that were then fixed in the PR.
why_stopped: >
  No wall-clock win on the path the patch actually changes. Alloc happens
  before discard/full decisions, so overload pays malloc+free for messages
  that never enqueue. Per-queue pEnqNode is easy to leak again if a later
  wait is added after handoff. Incomplete (MultiEnq still allocates under
  the lock). Not accepted as "better practice" hygiene in queue.c.
evidence:
  - this conversation's local paired sessions vs main 135fb6cfb and
    PR HEAD c9fbcfeb9 (2026-09-09/10); method in the performance skill
  - https://github.com/rsyslog/rsyslog/pull/7577
reopen_if: >
  A finished design allocates only when enqueue will proceed, has no
  queue-object side channel, covers MultiEnq, and two independent sessions
  meet the declared target on a LinkedList EnqMsg workload. Do not revive
  the current PR solely as style.
```

### diskqueue-persistent-read-descriptor

```yaml
id: diskqueue-persistent-read-descriptor
status: rejected
recorded: 2026-07-16
last_reviewed: 2026-09-10
stale_after: 2028-01-07
area: runtime segmented disk queue / DA
tried: Persistent validated read descriptor reuse to cut open/close syscalls.
result: >
  Open/close counts fell, but batch-1024 end-to-end and drain gains stayed
  below the 10% acceptance threshold.
why_stopped: Local syscall win did not meet the campaign's end-to-end bar.
evidence:
  - benchmarks/segmented-diskqueue/results/campaign-2026-07.json
  - .agent/skills/rsyslog-performance/references/campaign-lessons.md
reopen_if: >
  A workload where open/close dominates end-to-end time (not 1024-message
  batches on this host class) is measured under the same paired protocol.
```

### diskqueue-reserved-record-header-alloc

```yaml
id: diskqueue-reserved-record-header-alloc
status: rejected
recorded: 2026-07-16
last_reviewed: 2026-09-10
stale_after: 2028-01-07
area: runtime segmented disk queue / DA
tried: Reserved record-header allocation headroom.
result: Two sessions were noisy; no repeatable gain above the threshold.
why_stopped: Inconclusive under the campaign noise rules; not retained.
evidence:
  - benchmarks/segmented-diskqueue/results/campaign-2026-07.json
reopen_if: Repeatable paired sessions on current main beat the declared target.
```

### diskqueue-reuse-encoded-payload-on-rotation

```yaml
id: diskqueue-reuse-encoded-payload-on-rotation
status: rejected
recorded: 2026-07-16
last_reviewed: 2026-09-10
stale_after: 2028-01-07
area: runtime segmented disk queue / DA
tried: Reuse the encoded payload across segment rotation.
result: Pilot and rerun changed direction with high dispersion.
why_stopped: Unstable effect; rotation rewrite not kept.
evidence:
  - benchmarks/segmented-diskqueue/results/campaign-2026-07.json
reopen_if: Independent sessions agree on the same side of the target with MAD
  below the campaign inconclusive limit.
```

### diskqueue-timed-restart-replay-benchmark

```yaml
id: diskqueue-timed-restart-replay-benchmark
status: measurement-rejected
recorded: 2026-07-16
last_reviewed: 2026-09-10
stale_after: 2028-01-07
area: benchmarks/segmented-diskqueue
tried: >
  A timed clean restart/replay screen for segmented and DA queues.
result: >
  Safe designs either left action-owned records outside the tested queue,
  blocked clean shutdown, suppressed DA transfer, or added another
  persistent queue that confounded the metric.
why_stopped: >
  The benchmark could not isolate the intended cost. Functional restart and
  DA retry tests remain the guardrail; do not publish a confounded timer.
evidence:
  - benchmarks/segmented-diskqueue/README.md
  - benchmarks/segmented-diskqueue/results/campaign-2026-07.json
    (guardrails.restart_replay)
reopen_if: >
  A harness owns exactly one queue, preserves DA transfer, and still has
  deterministic ready/complete oracles without extra persistent queues.
```
