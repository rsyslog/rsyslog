# Queue contention lifecycle benchmark

Run from two separately configured and built source trees using the same
Ubuntu 26.04 development image and compiler flags. The driver alternates order,
discards one calibration pair, and records eleven paired lifecycle timings.
Each daemon starts with fresh state, accepts 100,000 messages through imdiag,
uses a four-worker FixedArray queue with 1024-message dequeue batches, attaches
a JSON tree, drains, shuts down, and passes exact message-ID delivery checks.

```
python3 benchmarks/queue-contention/compare.py --before /path/to/baseline \
  --after /path/to/candidate --output /path/to/ignored/session-1
```

`--pairs`, `--messages`, and `--trial-timeout` must be positive; the timeout
bounds each image setup and trial operation separately. Multi runs
also require positive worker, connection, and payload values. The driver first
resolves the image ID, then pins every trial to it. `result.json` records the
requested and completed pairs, fixed queue settings, image ID, and the
revision/dirty state of the harness and each build checkout. A failed or
in-progress session has `status: "incomplete"` and `summary_accepted: false`;
do not use it for a benchmark summary.

Repeat the session independently. Before measurement, declare an improvement
target (10% median lifecycle reduction here) and regression guardrail (5%).
Compare one factor at a time using adjacent isolated revisions. Report median
paired ratios and median absolute deviations; do not infer lock-hold time or
production throughput from these lifecycle timings. Host exclusivity and cache
state are uncontrolled. Raw logs and host paths belong in ignored artifacts.
This compact workload is a screening result, not broad performance acceptance;
small per-batch improvements may be hidden by startup and testbench overhead.

## Fixed-offered-rate latency observation

`compare-latency.py` is a separate workload and does not alter the primary
throughput trial. It uses one Python monotonic clock domain for timestamps made
immediately before payload framing and for complete-line observations in the
omfile sink. It separately records the timestamp-to-immediately-before-`sendall`
preparation interval and dispatch lateness. The reported latency includes that
preparation, `sendall`, omfile visibility, and reader scheduling. It is not a
callback or wire-service-time measurement.

```
python3 benchmarks/queue-contention/compare-latency.py --before /path/to/baseline \
  --after /path/to/candidate --output /path/to/ignored/latency-session \
  --offered-rate 10000 --connections 16 --before-scope global --after-scope local \
  --before-queue-size 1088192 --before-consumer-workers 10 \
  --after-queue-size 1000000 --after-consumer-workers 2 \
  --frontend-capacity 10000 --frontend-max 8 --worker-minimum 1
```

The driver alternates builds and requires the same input workers, connections,
offered rate, polling interval, synchronous omfile flush policy, dequeue batch,
and worker-minimum policy per side. Per-side queue/consumer/scope resources are
recorded rather than inferred. Every sample rejects missing or duplicate IDs,
invalid output, timestamp mismatch, send errors, rate drift above two percent,
dispatch lateness, timestamp-to-dispatch preparation, or completed reader
iteration intervals above 400 microseconds. These configured instrumentation
budgets are below the one-millisecond decision floor; they are not an aggregate
measurement-error bound. A delay inside or after `sendall`, the daemon, kernel,
or filesystem cannot be independently bounded by this observer. After clean
shutdown, the helper reparses the entire sink against the sender's exact
ID/timestamp manifest, including required payload padding, late duplicate/extra
lines, and trailing bytes. The healthy-output p99
guardrail is accepted only when every pair is valid and each satisfies
`after <= before + max(10% of before, 1 ms)`; paired p99 margin median and MAD
are reported as dispersion, not substituted for the per-pair rule.

Use the multi-producer screening workload for queue contention. It runs 16
concurrent sending threads/connections, 8 imtcp input workers, and 4 main-queue
consumers (configurable with `--consumer-workers 8`). Each trial checks exact
IDs, generator success, complete receiver line barrier, and clean daemon
shutdown. The primary metric is generation plus receiver line barrier; raw data
also separates generation, drain, full lifecycle, and post-shutdown exact-ID
verification time. The latter is deliberately excluded from the timing metric,
but a verification failure makes the trial fail. Drain polling uses 10 ms
intervals to reduce quantization. Start with 3 measured pairs, then expand if the effect is
clear. Keep the single-imdiag-producer workload as a low-contention guardrail.

```
python3 benchmarks/queue-contention/compare.py --before /path/to/baseline \
  --after /path/to/candidate --output /path/to/ignored/multi-screen \
  --workload multi --messages 1000000 --pairs 3
```

Run the exact-ID oracle selftest against a built testbench binary before a
baseline campaign. It must reject both fixture corruptions:

```
docker run --rm -u "$(id -u):$(id -g)" \
  -v /path/to/baseline:/rsyslog \
  -v "$(pwd)/benchmarks/queue-contention:/campaign:ro" \
  -w /rsyslog/tests rsyslog/rsyslog_dev_base_ubuntu:26.04 \
  bash /campaign/selftest.sh ./chkseq
```

`--queue-size`, `--dequeue-batch-size`, and `--worker-minimum` parameterize
the FixedArray configuration. Per-side `--before-queue-size` and
`--after-queue-size`, `--before-consumer-workers` and
`--after-consumer-workers`, and `--before-scope`/`--after-scope` make an S2
comparison explicit in `result.json`. Both scopes default to `global`; the
driver then emits neither `queue.scope` nor a `queue.local.*` parameter, so
the frozen S0 configuration stays unchanged. A side using `local` must also
supply `--{side}-frontend-size` and `--{side}-max-frontends`; those are passed
only to that local daemon. `--print-configuration` validates these arguments
and prints the resolved bounds without starting Docker, which is useful when
reviewing a campaign command.

For the S2 10K/1M matched capacity comparison, use the planned imtcp worker
front cap, not TCP connection count. `queue-size` means global capacity for
the control and backend capacity for the local candidate. The bounded local
reservation is `B + N * (F + D)`, so eight fronts with `F=10000`, backend
`B=1000000`, and `D=1024` require 1,088,192 slots. The global control uses
that total as its single queue size; the candidate uses eight FE workers and
two backend workers while the control uses ten global workers:

```
python3 benchmarks/queue-contention/compare.py --before /path/to/baseline \
  --after /path/to/candidate --output /path/to/ignored/10k-1m \
  --workload multi --messages 1000000 --pairs 3 --input-workers 8 \
  --connections 16 --worker-minimum 1 --dequeue-batch-size 1024 --payload 512 \
  --before-scope global --before-queue-size 1088192 --before-consumer-workers 10 \
  --after-scope local --after-queue-size 1000000 --after-consumer-workers 2 \
  --after-frontend-size 10000 --after-max-frontends 8
```

`--producer-mode balanced` uses all configured TCP connections. `--producer-mode
skew` retains the configured imtcp and consumer worker budgets but uses one
active TCP connection for the finite connection-skew control. imtcp's dynamic
scheduling means this does not establish hot producer identity. It is an MPMC
baseline only: it does not claim to instantiate inactive local fronts.

Pass `--impstats` only for a diagnostic run. It enables queue mutex-contention
statistics and copies the raw JSON `log.file` output into the result directory
for every trial. Those runs are not timing evidence: impstats changes the
workload and still does not provide actual batch-size distributions.

Configured dequeue sizes are not actual-batch evidence. imtcp may submit
variable batches, and this harness currently records no producer-submit or
queue-dequeue histogram. Add diagnostic instrumentation before making claims
about actual producer, dequeue, or output-request batch distributions.

## S0 mutation-fixture correction, 2026-09-14

The current built-in `parse_json` takes JSON and a destination path and returns a
status. The former one-argument fixture did not establish the claimed mutation.
Both trial scripts now call the two-argument form, require the expected nested
value before writing an ID, and abort on an unclean configuration. Missing
mutation therefore prevents exact-ID success. Treat the measurements below as
historical; do not compare them directly with the corrected workload or infer
that they measured this mutation. The S0 campaign establishes a fresh baseline.

The initial eight-front reference has 1,080,000 waiting slots. When front slots
are released at acquisition, its accepted-obligation bound also includes eight
active batches of 1024: use `--queue-size 1088192` for the obligation-matched MPMC
comparison. Allocation bytes and RSS must be reported separately.

## Local-queue S0 evidence, 2026-09-14

The [execution ledger](../../doc/ai/designs/local-queue-execution-ledger.md)
tracks stage acceptance and the frozen workload/guardrails.
[Normalized evidence](evidence/local-queue-s0/) retains paired samples, binary
hashes and diagnostic limitations. Both S0 runtimes are unchanged controls;
their ratio is not a local-queue speedup. Balanced and low controls have two
independent sessions; connection skew has a separate characterization session.
The small debugger-observed batch diagnostic demonstrates actual variable
counts, but its scheduling is perturbed and its timings are excluded.

## Measured results, 2026-09-05

The retained runtime candidate is `454b340a1`, compared with `8b8ffb19c`.
It combines constant-time FixedArray retirement and bounded deferred message
release. Measurements used GCC with `-g` in
`rsyslog/rsyslog_dev_base_ubuntu:26.04` (image ID
`sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09`)
on an x86-64 host with 28 logical CPUs. Both builds used identical configure
options. The host was non-exclusive and caches were uncontrolled.

Each row below has eleven alternating measured pairs after a discarded
calibration pair. Ratios are candidate/baseline; lower is better. The two final
multi-producer sessions were separated by sanitizer, portability, and
distribution validation. No other local checks overlapped measurements.

| Final candidate workload | Session | Median ratio | Ratio MAD |
| --- | --- | ---: | ---: |
| Multi-producer generation plus drain | 1 | 0.832291 | 0.017353 |
| Multi-producer generation plus drain | 2 | 0.832193 | 0.015111 |
| Multi-producer full lifecycle | 1 | 0.883916 | — |
| Multi-producer full lifecycle | 2 | 0.880685 | — |
| Single-producer full lifecycle | 1 | 1.011274 | 0.005241 |

The multi-producer trials used four million messages, 16 sending threads and
connections, eight configured imtcp input workers, and four queue consumers
(all four started). Queue capacity was 32768, batch size 1024, and generated
payload size 512 bytes plus the JSON tree. Every trial checked exact IDs and
proper termination. Reproduce with the command above using
`--workload multi --messages 4000000 --pairs 11`; the single-producer guardrail
uses the driver's defaults. The retained combination reduced measured work
time by about 16.8% in both sessions and stayed inside the 5% low-contention
regression guardrail. This is workload-specific evidence, with no per-factor
attribution or general production-throughput guarantee.

Earlier isolated lifecycle screens explain the retained scope:

| Isolated change | Session | Median ratio | Ratio MAD |
| --- | --- | ---: | ---: |
| FixedArray retirement | 1 | 1.000012 | 0.002498 |
| FixedArray retirement | 2 | 1.001346 | 0.003521 |
| Deferred release, before final shutdown correction | 1 | 1.010838 | 0.001791 |
| Deferred release, before final shutdown correction | 2 | 1.009940 | 0.002437 |
| Experimental waiter targeting | 1 | 1.229120 | 0.024719 |
| Experimental waiter targeting with reservation cap | 1 | 1.230348 | 0.010424 |

FixedArray retirement remains an O(batch-size) to O(1) simplification with
neutral isolated lifecycle measurements. The waiter experiments were rejected
because of their throughput regression and are excluded from the candidate.
An earlier combined result predating the shutdown correction is not used as a
final-candidate acceptance session.

Deferred release preserves queue admission counts but can increase physical
live-payload retention: each worker may retain one completed batch while its
released capacity is reused. The extra reference count is bounded by the sum
of worker batch capacities, plus one pointer buffer per worker; payload bytes
depend on message and JSON/property sizes. References are drained before the
next action callback or idle/minbatch wait. Final validation covered broad
Ubuntu 26.04 tests, the static analyzer, ASan/UBSan, TSan, both compiler
portability builds, mock distcheck, and deterministic enqueue/shutdown and
out-of-order-retirement regressions.
