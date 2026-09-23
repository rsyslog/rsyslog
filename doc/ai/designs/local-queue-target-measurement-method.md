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
[implementation plan](local-queue-implementation-plan.md). The target-environment sections remain prospective; the sustained local screen
and results below are completed evidence. The maintainer excluded a resource-heavy
Elasticsearch campaign from bounded S7 on 2026-09-15.

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


## Completed sustained contention screen (S7)

This method was discovered in session `01a0a5dd-878e-7791-93de-adc60b44b359`
and applied to immutable baseline/S3/S6 builds on 2026-09-15. Unlike the earlier
short omfile/omfwd screens, it establishes substantial shared-queue contention.
It complements those delivery-checked tests; it does not replace them.

### Reproduce the stimulus

Use matching optimized builds (GCC `CFLAGS=-O2 -g`) and run one daemon at a time.
The measured queue is directly downstream of imtcp, not behind an unmeasured
shared main queue. Use 8 imtcp workers and 32 parallel TCP connections, 64 extra
payload bytes, 100,000-message bursts with no intentional gap, and an actual
high-resolution deadline at least 60 seconds after traffic starts. Each completed
burst increments both the sent total and the next starting message ID:

```sh
tests/tcpflood -s -t127.0.0.1 -p"$port" -c32 -Y   -i"$sent_messages" -m100000 -d64
```

`$port` is the daemon's loopback listener; `sent_messages` starts at zero and
increases by 100000 after each successful invocation. Check sender exit status.
The repeated invocation/connections are part of the stimulus: this is not a
model of thousands of permanently connected production clients. A burst can
finish beyond the 60-second deadline; report measured duration and actual count.

Queue settings are FixedArray, BE size 2,000,000, 16 BE workers, worker minimum 1,
dequeue batch maximum 1, light-delay mark 2,000,000, full-delay mark 1,999,999,
and mutex contention stats on. Locals add scope local, FE size 10,000 or 100,000,
maxFrontends 8, frontendStats on, helperBatchSize 1. Leave high/low/discard marks
implicit: S3 rejects those explicit parameters. No disk is configured.
Global uses the same queue settings without the local parameters.

Use the same per-message work on every revision:

```rsyslog
template(name="jsonout" type="list" option.jsonftree="on") {
    property(outname="meta.msg" name="$!meta!msg" format="jsonf")
    property(outname="meta.hostname" name="$!meta!hostname" format="jsonf")
    property(outname="meta.tag" name="$!meta!tag" format="jsonf")
    property(outname="meta.timestamp" name="$!meta!timestamp" format="jsonf")
    property(outname="meta.severity" name="$!meta!severity" format="jsonf")
    property(outname="meta.facility" name="$!meta!facility" format="jsonf")
    property(outname="msg" name="$!meta!msg" format="jsonf")
}
if ($msg contains "msgnum:") then {
    set $!meta!msg = $msg & " queue-contention-mutated";
    set $!meta!hostname = $hostname;
    set $!meta!tag = $syslogtag;
    set $!meta!timestamp = $timereported;
    set $!meta!severity = $syslogseverity-text;
    set $!meta!facility = $syslogfacility-text;
    action(type="omfile" file="/dev/null" template="jsonout"
           zipLevel="0" flushOnTXEnd="on" asyncWriting="off" ioBufferSize="64k")
}
```

Use the default zlib driver and compression off for S3's configuration gate.
`/dev/null` bypasses actual writing and compression; the earlier zstd9/null
configuration was never a compression benchmark. Disable internal-message
processing, and log impstats directly to a dedicated file every second using
JSON and `log.syslog="off"`. Validate each configuration with `-N1`.

### Completion and measurements

Parse JSON from the first `{` on timestamp-prefixed impstats lines. Require
successful sender and daemon exits, action processed = sent, failed/discarded = 0,
and BE size = 0. For locals also require accepted = terminal = sent and logical
outstanding = 0. The retained harness checks two settled polls 1.1 seconds apart;
its drain time includes this observation delay. Never terminate the measurement
based only on BE emptiness. Null-output counters cannot verify individual IDs,
duplicates, ordering, or content; use the existing exact-ID omfwd tests for that
separate evidence.

Report sent/(traffic + drain), sending rate, FE/BE routing, helper completions,
peak occupancy, CPU and RSS. Sample process CPU/RSS every 250ms. Linux proc stat
RSS is in pages, even though the original sampler header said KiB; convert using
the host page size. Normalize contention events and acquisition wait by processed
messages. Wait is cumulative across workers, includes scheduler delay and only
covers instrumented lock sites. Stats/trylock overhead is part of these runs.
Do not describe it as lock hold time or a wall-clock percentage.

### Results and interpretation

| Configuration | Processed | Traffic / drain seconds | Messages/s including drain | Versus global baseline | FE route share |
|---|---:|---:|---:|---:|---:|
| Global FixedArray | 30.0M | 60.589 / 5.618 | 453,118 | 1.00x | — |
| S3 FE10K | 64.8M | 60.072 / 6.770 | 969,453 | 2.14x | 77.22% |
| S6 FE10K | 63.8M | 60.066 / 9.031 | 923,336 | 2.04x | 75.95% |
| S3 FE100K | 71.9M | 60.852 / 9.091 | 1,027,978 | 2.27x | 76.85% |
| S6 FE100K | 65.2M | 60.058 / 9.039 | 943,614 | 2.08x | 77.44% |

The original global LinkedList reproduction processed 16M at 215,357 messages/s
including drain, with 3,046,959 contended acquisitions and 78.546 seconds of
cumulative wait. It confirms contention but is not the same-storage denominator
for the table. S3 requires FixedArray for local scope.

Contended acquisitions per million messages: baseline 279,515; S3/S6 FE10K
77,947/79,169; S3/S6 FE100K 72,969/72,850. Wait microseconds per message:
4.459; 5.766/5.743; 4.083/4.713 respectively. Fewer contention events do not imply
less time waiting in every configuration. Both FE10K raw final counters happened
to equal 5,050,951; a narrow code check found no cap, and no causal interpretation
is assigned to this equality.

FE100K improved S3/S6 by 6.0%/2.2% over FE10K, while peak RSS rose from about
1,649/1,654 MiB to 2,205/2,208 MiB. Route share barely changed. A small single-run
difference is not proof of a stable gain or regression. Each cell has one run;
the 100K runs reused the earlier baseline/10K results. No paired confidence or
production latency claim is made.

Local runs add eight FE workers to the same sixteen BE workers. Their capacity
bounds are 2,080,008 (10K) and 2,800,008 (100K), versus global 2,000,000. These
are fixed-BE comparisons, not equal-total-memory or equal-worker comparisons.
Batch maximum one deliberately stresses synchronization. Practical larger-batch,
equal-resource, repeated and real-destination measurements remain optional future
work; they are not required by the maintainer's narrowed S7.

Revisions: baseline `b0d9f971f007f06db3f734543cef5dfb312c5090`, S3
`58570a0fd39f49e00f90199ce24bd87e04bbe021`, S6
`31796c8e1a4e0933942e7a21fd1618f2cf898098`. Same Ubuntu26.04 image digest:
`sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09`.
All completion counter checks passed. Baseline is the original common ancestor,
newly measured, not current upstream main or a final-S6 global regression test.

Raw configs, commands, counters, samples, binary/build identities and scripts
are retained at `/home/rger/rsyslog-local-queue-artifacts/contention-comparison/`:
`final-first-set/` holds the matched 10K comparison and `fe100k/` the follow-up.
The table, stimulus and oracle above are the portable record; the local raw
artifact directory is not part of the source distribution.
