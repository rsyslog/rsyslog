# Campaign lessons

The first evidence source was the segmented disk queue and DA spill/drain
campaign in `benchmarks/segmented-diskqueue/`.

Transferable findings:

- Reusing already-computed integrity-accounting values and replacing a small
  portable lookup implementation with a byte-table implementation produced
  repeatable gains for large payloads without changing integrity coverage.
- Apparent local wins did not guarantee end-to-end value. Persistent descriptor
  reuse reduced open/close calls but missed the acceptance threshold in
  realistic 1024-message batches.
- Allocation headroom and rotation rewrite candidates were reverted because
  their results were noisy or inconsistent across sessions.
- A duplicate decode copy named as a candidate was already absent. Source
  inspection prevented a needless change.
- Full startup and shutdown remained mandatory in every trial. An unrecorded
  calibration pair reduced one-time setup effects but did not control cache or
  scheduler state.
- Several timed restart/replay designs changed queue ownership or shutdown
  behavior and therefore could not isolate the intended metric. The campaign
  kept functional recovery tests and reported the timing gap instead of
  publishing a confounded benchmark.
- Large 32-KiB messages exposed gains that 512-byte and 4-KiB workloads could
  not. Multiple representative sizes and explicit targeted metrics were
  necessary to avoid optimizing only the smallest common case.

Read the tracked normalized report in the benchmark suite for exact ratios,
dispersion, revisions, and validation limitations. Do not generalize a
component-specific result without new measurements in the target subsystem.
