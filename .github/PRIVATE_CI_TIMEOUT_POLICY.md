# Private CI timeout policy

Public workflows retain their existing timeout values. Private repositories
route Linux work through `RSYSLOG_CI_LINUX_RUNNER`; all healthy matching workers
remain eligible and matrix parallelism is unchanged.

Explicitly bounded private build, package, test, fuzzer-build and container-build
jobs receive 180 minutes. This includes Ubuntu/RPM packages, the main Linux
matrix, Kafka distcheck, i386, compiler jobs and standalone service/fuzz jobs.
The existing 180-minute dev-container job already meets this budget.
Jobs without an explicit timeout retain GitHub's 360-minute default; this
policy does not lower their allowance. The change-scope and read-only flake
harvester jobs retain their short 10-minute budgets because they do not build
or execute the testbench. Hosted/native/release jobs outside private routing
retain their existing policies.

The i386 aggregate check command receives 150 minutes privately, within its
180-minute job; public execution retains the 45-minute check and 75-minute job.
Per-test watchdogs, fuzzer campaign duration and per-input limits, network
request limits, test selection and failure handling remain unchanged.

These are bounded initial operating allowances for heterogeneous shared workers,
not measured completion guarantees. A hard timeout is incomplete validation,
never a test pass. Inspect assignment, job annotations, logs, CPU scheduling,
I/O, memory and free disk before retrying. Retain exact-head completion results
and revisit the budget if jobs still reach it. Do not bypass failing tests,
disable healthy slow workers or serialize the entire matrix onto one worker.

New policy affects only runs using the updated workflow revision. Rerunning an
older SHA retains its older limits. Coordinate adoption with active branch
owners; do not rewrite their approved heads or interrupt busy jobs. During
release-critical validation, prepare follow-ups locally to avoid filling the
shared runner queue with an unrelated full matrix.
