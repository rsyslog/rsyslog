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

Repeat the session independently. Before measurement, declare an improvement
target (10% median lifecycle reduction here) and regression guardrail (5%).
Compare one factor at a time using adjacent isolated revisions. Report median
paired ratios and median absolute deviations; do not infer lock-hold time or
production throughput from these lifecycle timings. Host exclusivity and cache
state are uncontrolled. Raw logs and host paths belong in ignored artifacts.
This compact workload is a screening result, not broad performance acceptance;
small per-batch improvements may be hidden by startup and testbench overhead.

Use the multi-producer screening workload for queue contention. It runs 16
concurrent sending threads/connections, 8 imtcp input workers, and 4 main-queue
consumers (configurable with `--consumer-workers 8`). Each trial checks exact
IDs, generator success, complete drain, and clean daemon shutdown. The primary
metric is generation plus drain; raw data also separates generation, drain,
and full lifecycle. Drain polling uses 10 ms intervals to reduce quantization. Start with 3 measured pairs, then expand if the effect is
clear. Keep the single-imdiag-producer workload as a low-contention guardrail.

```
python3 benchmarks/queue-contention/compare.py --before /path/to/baseline \
  --after /path/to/candidate --output /path/to/ignored/multi-screen \
  --workload multi --messages 1000000 --pairs 3
```

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
