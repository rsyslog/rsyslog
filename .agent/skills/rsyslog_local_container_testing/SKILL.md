---
name: rsyslog_local_container_testing
description: Mirror rsyslog run_checks.yml container validation locally, including the clang static analyzer job followed by the Ubuntu 26.04 run-ci.sh check run, service-skip validation, clean-tree rules, and container path caveats.
---

# rsyslog_local_container_testing

Use this skill when validating rsyslog implementation changes through local dev
containers, and treat it as the final pre-finish gate when Docker, Podman, or
equivalent container tooling is available. Local container validation should
mirror the relevant GitHub Actions container jobs instead of inventing a
parallel local-only flow. If container tooling is unavailable, report that
limitation explicitly rather than silently substituting a weaker gate.

## When To Use

Use this for PR-ready validation whenever local container support is available.
It is often faster than waiting for GitHub CI to discover environment-specific
failures, especially static-analyzer findings, compiler/toolchain differences,
dependency differences, generated build state issues, and service-test skip
behavior.

Focused host-side tests from `rsyslog_test` are still the right first step when
debugging a specific behavior. After the patch is stable, run this skill before
pushing if the change is intended for review.

## Pre-PR Testbed Tiers

Use these tiers as a session testbed before opening or updating PRs. The goal is
to catch the failures that otherwise cost a full GitHub Actions turnaround and
follow-up babysitting cycle, without cloning the entire CI matrix for every
small patch.

- **Tier 1: default PR-ready gate for code or testbench changes**. Run the
  existing static-analyzer pass and Ubuntu 26.04 `devtools/run-ci.sh` check.
  This is the normal local confidence gate for C, parser, module, runtime,
  `tests/*.sh`, `diag.sh`, `Makefile.am`, `configure.ac`, and
  `run_checks.yml` changes.
- **Tier 2: compile portability gate for non-trivial C/header changes**. Add
  the two build-only compile lanes from `run_checks.yml`:
  `clang21-ndebug` and `gcc15-gnu23-debug`. Use this when a change touches
  shared headers, compiler-sensitive code, generated C paths, configure/build
  inputs, or anything that could behave differently under debug/no-debug or
  GNU23 compilation.
- **Tier 3: risk-triggered specialist lanes**. Add only the lanes that match the
  touched area or risk profile:
  `ubuntu_26_san` for memory, parser, string, JSON, property, and bounds work;
  `ubuntu_26_tsan` for threading, queue, action lifecycle, or lock-order work;
  `ubuntu_26_imtcp_no_epoll` for imtcp and network input changes;
  `i386_CI` for integer width, pointer size, serialization, and ABI-sensitive
  work; `ubuntu_22_distcheck` or `kafka_distcheck_CI` for dist, autotools,
  packaging, or Kafka Makefile changes.

Keep macOS, full distro matrix, package builds, and service-backed matrix jobs
as CI-owned by default. Run them locally only when the change directly targets
that area and the local host can reproduce the required environment.

## Preferred Order

For the full final gate, run two container validations for broad local
confidence. Mirror these
`.github/workflows/run_checks.yml` paths:

1. The `clang static analyzer` job's container command.
2. The Ubuntu 26.04 `devtools/run-ci.sh` check run, only after the analyzer run
   is clean or its findings are understood.

Keep runtimes and summarize failures separately for each run.

## Clean Tree Rule

Before every container-mode switch, clean generated build state and rebuild from
a fresh configure/build cycle. This includes switching compiler, sanitizer
flags, configure options, container image, test mode, analyzer mode, or
returning from a container run to host-side testing:

```sh
make distclean || true
rm -f tests/runtime_unit_gss_token_util \
  tests/runtime_unit_linkedlist \
  tests/runtime_unit_stringbuf
```

Skip this only for an immediate rerun with unchanged settings in the same
container mode. If the lane changes, reproducibility is more important than
incremental build speed.

Container configure runs leave generated Makefiles with absolute paths from
inside the container, such as `/rsyslog/missing`. Reconfigure on the host before
running host-side `make` in the same tree.

`make distcheck` can leave extracted `rsyslog-<version>/` trees with read-only
directory permissions. If a normal cleanup gets `Permission denied` but the tree
is still owned by the local user, do not use `sudo rm`. Restore owner-write
permission on the generated tree and remove it as the normal user:

```sh
chmod -R u+w rsyslog-*/
rm -rf rsyslog-*/
```

If the tree is actually owned by root because a container ran with the wrong
user mapping, first fix ownership on that generated tree only, then remove it as
the normal user:

```sh
sudo chown -R "$(id -u):$(id -g)" rsyslog-*/
chmod -R u+w rsyslog-*/
rm -rf rsyslog-*/
```

## Run 1: run_checks.yml Clang Static Analyzer

Clean before analyzer mode so `scan-build` recompiles the tree:

```sh
make distclean || true
rm -rf scan-build-report clang-analyzer.log
```

Run:

```sh
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
SCAN_BUILD='scan-build' \
SCAN_BUILD_CC='clang' \
SCAN_BUILD_REPORT_DIR='scan-build-report' \
CI_MAKE_OPT='-j20' \
DOCKER_RUN_EXTRA_OPTS='-e SCAN_BUILD -e SCAN_BUILD_CC -e SCAN_BUILD_REPORT_DIR' \
RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--disable-elasticsearch --disable-elasticsearch-tests --disable-imkafka --disable-omkafka --disable-kafka-tests --disable-mysql --disable-mysql-tests' \
devtools/devcontainer.sh --rm devtools/run-static-analyzer.sh 2>&1 | tee clang-analyzer.log
```

The analyzer job intentionally has its own concurrency knob. In `run_checks.yml`
it uses `CI_MAKE_OPT='-j20'`; do not infer this from the broad compile matrix.

## Optional Tier 2: run_checks.yml Compile Matrix

Run these before PR for non-trivial C/header or portability-sensitive changes.
They are build-only checks and normally finish faster than a failed PR cycle.
Clean generated state before switching between configurations.
These snippets intentionally set `RSYSLOG_CONTAINER_UID=''` to mirror CI/root
container execution. Some CI-equivalent lanes need root semantics for service
startup and sudo-backed setup. The tradeoff is that they can leave root-owned
generated files on the host-side worktree; use the clean-tree and ownership
recovery rules above before switching lanes or returning to host-side testing.

For `clang21-ndebug`:

```sh
make distclean || true
rm -f tests/runtime_unit_gss_token_util \
  tests/runtime_unit_linkedlist \
  tests/runtime_unit_stringbuf
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
RSYSLOG_CONTAINER_UID='' \
CI_CONFIGURE_CACHE='1' \
CC='clang-21' \
CFLAGS='-g' \
RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--enable-debug=no' \
devtools/devcontainer.sh --rm devtools/run-configure.sh
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
RSYSLOG_CONTAINER_UID='' \
devtools/devcontainer.sh --rm make -j80
```

For `gcc15-gnu23-debug`:

```sh
make distclean || true
rm -f tests/runtime_unit_gss_token_util \
  tests/runtime_unit_linkedlist \
  tests/runtime_unit_stringbuf
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
RSYSLOG_CONTAINER_UID='' \
CI_CONFIGURE_CACHE='1' \
CC='gcc-15' \
CFLAGS='-g -std=gnu23' \
RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--enable-debug=yes --disable-omamqp1' \
devtools/devcontainer.sh --rm devtools/run-configure.sh
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
RSYSLOG_CONTAINER_UID='' \
devtools/devcontainer.sh --rm make -j80
```

## Run 2: run_checks.yml Ubuntu 26.04 Check

Use the same `devtools/run-ci.sh` path as CI. On this host, use `-j80`.
For full Valgrind-capable check runs, set a non-sanitizer compiler and CFLAGS;
ASAN-instrumented binaries are not suitable for Valgrind tests.

For service-skip validation with an irrelevant fake change set:

```sh
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
RSYSLOG_TESTBENCH_CHANGED_FILES='doc/README.md' \
CC='clang-21' \
CFLAGS='-g' \
CI_MAKE_OPT='-j80' \
CI_MAKE_CHECK_OPT='-j80' \
CI_CHECK_CMD='check' \
devtools/devcontainer.sh --rm devtools/run-ci.sh
```

This run intentionally leaves locally available service test families enabled
unless the CI-equivalent configure path disables them. For an irrelevant fake
change set, MySQL, libdbi, and Kafka should appear in the test list but skip via
`tests/diag.sh` before service startup. This validates the in-test gate that
local container users rely on.

For broad local runs, prefer abort-on-first-failure plus an outer timeout so an
already-invalid lane does not keep consuming local time:

```sh
timeout 1800 env \
ABORT_ALL_ON_TEST_FAIL='YES' \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
CC='clang-21' \
CFLAGS='-g' \
CI_MAKE_OPT='-j80' \
CI_MAKE_CHECK_OPT='-j80' \
CI_CHECK_CMD='check' \
devtools/devcontainer.sh --rm devtools/run-ci.sh
```

Any full-lane failure is an alert condition. Do not dismiss it as unrelated or
flaky without either a clean retry or a concrete root cause from logs. When
retrying, keep `ABORT_ALL_ON_TEST_FAIL='YES'` and the outer timeout so the retry
stops at the first actionable failure instead of hiding it behind later noise.

For a faster build-only smoke check, set `CI_MAKE_CHECK_OPT='-j80 TESTS='`.
This is useful for intermediate feedback, but it does not satisfy the full
final validation gate.

## Optional Tier 3: Risk-Triggered Specialist Lanes

Run these only when the changed area justifies the extra local time. The command
shape should stay close to `run_checks.yml`; record deviations in the final
status.

- **Memory-sensitive changes**: mirror `ubuntu_26_san` with `clang-21`,
  AddressSanitizer, UndefinedBehaviorSanitizer, disabled Valgrind, and the same
  configure extras. Use for parser, property, string, JSON, bounds, and
  segfault-related work.
- **Threading changes**: mirror `ubuntu_26_tsan` with `clang-21`,
  ThreadSanitizer, `--security-opt seccomp=unconfined`, and the CI TSAN
  suppressions. Use for queues, worker lifecycle, action shutdown, locks, and
  shared state.
- **imtcp select-mode changes**: mirror `ubuntu_26_imtcp_no_epoll` when touching
  imtcp behavior that could differ between epoll and poll/select.
- **32-bit or ABI-sensitive changes**: mirror `i386_CI` when changing integer
  sizes, pointer casts, serialization, protocol framing, or disk/state formats.
- **Distribution or build-system changes**: mirror `ubuntu_22_distcheck` for
  general dist/autotools risk, and `kafka_distcheck_CI` for Kafka build/test
  plumbing.

Do not run these by habit. The expected value is highest when the lane is tied
to the changed subsystem or to a specific failure mode seen during babysitting.

## Service Relevance

Heavy service-backed tests self-skip through `tests/diag.sh` when changes do not
touch their relevant module paths or `runtime/*.[ch]`. Use
`RSYSLOG_TESTBENCH_CHANGED_FILES` for fake change sets and
`RSYSLOG_TESTBENCH_FORCE_SERVICE_TESTS=1` or per-family force variables when
intentionally validating service tests without relevant source changes.

## Reporting

Report:

- exact command shape and container image
- elapsed `real` time from `/usr/bin/time -p` where used
- pass/skip/fail summary from `tests/test-suite.log`
- whether skips were expected from relevance filtering
- unrelated local flakes separately from failures caused by the change
