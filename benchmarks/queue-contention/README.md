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
