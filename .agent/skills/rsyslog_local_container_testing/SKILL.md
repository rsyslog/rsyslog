---
name: rsyslog_local_container_testing
description: Mirror rsyslog run_checks.yml container validation locally, including the clang static analyzer job followed by the Ubuntu 26.04 run-ci.sh check run, service-skip validation, clean-tree rules, and container path caveats.
---

# rsyslog_local_container_testing

Use this skill when validating rsyslog changes through local dev containers.
Local container validation should mirror the relevant GitHub Actions container
jobs instead of inventing a parallel local-only flow.

## When To Use

Use this for PR-ready validation whenever local container support is available.
It is often faster than waiting for GitHub CI to discover environment-specific
failures, especially static-analyzer findings, compiler/toolchain differences,
dependency differences, generated build state issues, and service-test skip
behavior.

Focused host-side tests from `rsyslog_test` are still the right first step when
debugging a specific behavior. After the patch is stable, run this skill before
pushing if the change is intended for review.

## Preferred Order

Run two container validations for broad local confidence. Mirror these
`.github/workflows/run_checks.yml` paths:

1. The `clang static analyzer` job's container command.
2. The Ubuntu 26.04 `devtools/run-ci.sh` check run, only after the analyzer run
   is clean or its findings are understood.

Keep runtimes and summarize failures separately for each run.

## Clean Tree Rule

Before switching compiler, sanitizer flags, configure options, container image,
or test mode, clean generated build state:

```sh
make distclean || true
rm -f tests/runtime_unit_gss_token_util \
  tests/runtime_unit_linkedlist \
  tests/runtime_unit_stringbuf
```

Skip this only for an immediate rerun with unchanged settings.

Container configure runs leave generated Makefiles with absolute paths from
inside the container, such as `/rsyslog/missing`. Reconfigure on the host before
running host-side `make` in the same tree.

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

For a faster build-only smoke check, set `CI_MAKE_CHECK_OPT='-j80 TESTS='`.

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
