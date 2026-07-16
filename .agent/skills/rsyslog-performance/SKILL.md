---
name: rsyslog-performance
description: Safely optimize rsyslog runtime performance with representative workloads, isolated baseline and candidate builds, profiling, paired noise-resistant measurements, explicit acceptance thresholds, and correctness/security validation. Use for performance investigations or implementations in inputs such as imtcp, parsers, outputs, queues, and other runtime subsystems.
---

# Rsyslog Performance Optimization

Use the canonical human procedure in
[performance_optimization.rst](../../../doc/source/development/performance_optimization.rst).
Apply this skill to execute that procedure; do not replace or duplicate it.

## Establish the contract

1. Read the repository and subtree `AGENTS.md` files and applicable build,
   test, module, documentation, and container-validation skills.
2. Write down observable behavior plus security, durability, compatibility,
   ownership, memory, and concurrency invariants before changing code.
3. Define representative payload sizes, batch sizes, concurrency, failure
   modes, lifecycle phases, targeted metrics, and regression guardrails.
4. Choose improvement and regression thresholds before collecting results.
5. Identify approval boundaries. Stop at an evidence-backed proposal before
   weakening safety or changing durability, persistence, ownership, bounds,
   validation, state machines, locking, retry behavior, or public interfaces.

Prefer the simplest candidate that can plausibly matter. Do not optimize a
path merely because it looks inefficient; profile or measure it first.

## Build trustworthy evidence

1. Refresh the upstream base and create separate baseline and candidate
   sibling worktrees. Never build in the coordination checkout.
2. Locate an existing subsystem benchmark under `benchmarks/`. If none exists,
   add the smallest reusable, Git-tracked harness that can reproduce the path.
   Keep raw artifacts ignored and normalized, machine-neutral evidence tracked.
3. Include the full system lifecycle when startup, shutdown, persistence, or
   recovery affects the path. Use deterministic readiness and completion
   signals and fresh per-trial state.
4. Profile timing, CPU, allocations, syscalls, lock contention, and call paths
   as applicable. Select one independently measurable candidate.
5. Run one unrecorded baseline/candidate calibration pair, then at least eleven
   measured pairs with alternating order on a non-exclusive host. Compare
   paired ratios using their median and median absolute deviation.
6. Repeat the complete session independently, preferably later. Rerun one
   noisy workload under steadier conditions; reject it as inconclusive if the
   effect still cannot be separated from host noise.

Never claim calibration creates a controlled cold cache. Do not drop global
caches or disrupt unrelated host processes.

## Change one factor

Implement one safe candidate at a time. Preserve error paths and cleanup while
checking malformed, oversized, retry, shutdown, and partial-failure behavior.
Run focused validation before benchmarking so invalid candidates do not consume
measurement time.

Retain a candidate only when both independent sessions meet the declared
target and every guardrail. Revert neutral, noisy, unsafe, overly complex, or
questionable changes. Record rejected candidates because they prevent repeated
low-value work.

## Validate and report

Run formatting, focused tests, test-antipattern checks, relevant compiler and
sanitizer lanes, static analysis, ownership/concurrency audits, documentation
checks, local Cubic when available, and PR-ready container validation. Match
coverage to risk: buffer and bounds changes need ASAN/UBSAN; queue, worker,
locking, and shutdown changes need TSAN.

Report:

- exact revisions, workload definitions, commands, compiler settings, and
  concurrency;
- paired medians, dispersion, raw metric summaries, regressions, and outliers;
- retained, rejected, and inconclusive candidates;
- safety and compatibility invariants checked;
- validation commands, container identity, skipped coverage, and limitations.

For a concise example of evidence that shaped this workflow, read
[campaign-lessons.md](references/campaign-lessons.md). Keep component-specific
commands and detailed results in that subsystem's benchmark directory.

## Extend the method

After a later campaign passes its gates, add only transferable lessons,
measurement techniques, validation routing, or a concise case-study reference.
Do not turn this skill into a component manual. Keep the developer page
canonical and route future work to the appropriate subsystem benchmark suite.
