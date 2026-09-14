<!--
.. meta::
   :description: Target-environment method for measuring local queues with persistent TCP inputs and Elasticsearch output.
   :keywords: rsyslog, local queue, Elasticsearch, performance, queue backlog, batching
-->

# Local queue target measurement method

<!-- .. summary-start -->
This method separates local queue mechanism diagnostics from target-environment
measurement with persistent TCP clients and Elasticsearch output. It preserves
correctness evidence and reports delivery, queue, batch, CPU, and output outcomes
without treating a local omfile screen as production evidence.
<!-- .. summary-end -->

## Scope and current evidence

This complements the [execution ledger](local-queue-execution-ledger.md) and
[implementation plan](local-queue-implementation-plan.md). It is a prospective
method, not an S2 acceptance result.

The user reports a target system of roughly 80 cores with a complex rsyslog
configuration. This is provisional planning context, not a measured hardware or
configuration fact. Capture the effective configuration and measured inventory
before selecting worker budgets or interpreting a comparison.

The existing queue-contention harness has exact post-shutdown ID checks and is
useful for configuration, corruption, and controlled queue-mechanism diagnostics.
Its static omfile output, local loopback TCP, and synthetic JSON mutation do not
represent Elasticsearch bulk requests, retries, cluster latency, or production
backpressure. A controlled output delay tests a queue mechanism only.

The retained local latency artifact has 100,000 exact final IDs but is invalid for
its local Python observation contract: maximum sender dispatch and completed reader
iteration exceeded the fixed 400 microsecond limit. Its observed p99 is therefore
not a latency acceptance result. A future target method may use a different,
predeclared observation boundary and uncertainty method; it must label that metric
as incomparable with the local observer result.

## Target workload and qualification

Use a bounded native or event-loop sender, not one Python thread per connection.
It must sustain persistent TCP connections, record successful connects and
reconnects, assign exact message IDs, and retain scheduled and actual send timing
plus blocked-send outcomes. The sender and rsyslog must have separately recorded
CPU resources so generator saturation is visible.

Start with provisional aggregate profiles of 500 and 1,000 persistent clients at
5,000 and 10,000 messages per second (300,000 and 600,000 per minute). Before any
replicated comparison, record every qualification attempt for all selected profiles:
exact delivery, actual offered rate, client stability, queue reconciliation,
observation validity, and fixed Elasticsearch cluster/index health. Freeze one
warmup duration and one steady measurement window per profile after qualification;
do not choose them per sample. Burst and uneven-client shapes require fixed,
recorded schedules. Uneven clients do not prove a hot frontend until producer
identity is independently established.

## Matched resources and required records

For each compared pair, keep the compiler/image, input-worker count, CPU set,
cgroup CPU and memory limit, queue slot budget, payload, sender schedule, and
Elasticsearch endpoint, index, shard, replica, refresh, and bulk policy equal.
Match the configured processing-worker budget: global mode uses `W` workers and
local mode uses a configured FE-plus-BE budget of `W`.

Report configured frontend slots, registered frontends, started/running FE workers,
BE workers, and global workers separately. A frontend slot is an allocation bound,
not a worker. Record the actual inventory at warmup completion, throughout the
steady window, and at quiescence: imtcp workers, queue workers, action/output
workers, helper/module workers, and their CPU-set and affinity placement. Record
the effective configuration fingerprint and relevant ruleset/action queue topology
without retaining credentials or payloads.

Also retain cgroup CPU and memory deltas, PSI where available, context switches,
mutex wait count/time, and CPU topology: sockets, NUMA nodes, cores, SMT layout,
memory-node placement, and the CPU/NUMA placement of rsyslog, the sender, and
Elasticsearch. Results from one heterogeneous WSL host do not establish scaling
for larger servers.

## Outcomes and denominators

Every run requires exact-ID integrity after shutdown. Keep logical accounting and
delivery accounting distinct:

- Record logical ingress, admission, route, terminal retirement, discard, and
  FE-to-BE transfer outcomes for queue correctness and reconciliation.
- Record Elasticsearch bulk requests, items and bytes; request response p50/p95/p99;
  completed bulk items; successful indexed items; retries; rejections; and failures.
- Use rsyslog cgroup CPU seconds per **successful Elasticsearch bulk-item
  completion** as the delivery CPU denominator. Report daemon CPU separately from
  Elasticsearch CPU when the latter is available. Never use generic queue terminal
  retirement as a successful-delivery denominator because terminal accounting can
  include discard paths.

At a fixed offered rate, elapsed completion is diagnostic. It shows whether delivery
kept pace, but is mostly sender-paced. Primary target records are CPU per successful
item, successful delivery fraction, retry/rejection behavior, queue backlog and age,
and valid latency measures. A finite increasing backlog slope is a condition to
investigate, not proof of unbounded growth. Claim sustained growth only after
predeclared consecutive steady windows and replicated target evidence.

Record actual batch-size distributions, not only configured ceilings, for imtcp
submission, FE dequeues, BE leases, and Elasticsearch bulk requests. Count/sum/max
snapshots are useful summaries but cannot reconstruct a distribution; use a bounded,
off-timed diagnostic histogram or trace phase.

Daemon-local queue age and Elasticsearch request duration may use local monotonic
clocks. Do not call either end-to-end indexed-document latency without a proven
cross-host correlation method.
