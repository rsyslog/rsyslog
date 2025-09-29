# AGENTS.md – Testbench subtree

This guide covers everything under `tests/`, including shell test cases,
helpers, suppressions, and supporting binaries. Pair it with
[`tests/README`](./README) for human- and CI-facing run instructions; this
document focuses on authoring guidance and the knobs that matter most to AI
agents.

## Purpose & scope
- The directory implements the Automake testbench that exercises rsyslog.
- Each `.sh` script is a standalone scenario that can be executed directly or
  through `make check`.
- Use this guide together with the top-level `AGENTS.md` and the component
  guide that matches the module you are testing.

## Writing & updating tests
- Base new shell tests on existing ones; include `. $srcdir/diag.sh` to gain the
  helper functions (timeouts, Valgrind integration, rsyslogd launch helpers).
- Prefer harness helpers such as `cmp_exact`, `command_deny`, and
  `require_plugin` over ad-hoc shell to keep diagnostics uniform.
- Name Valgrind-enabled wrappers with the `-vg.sh` suffix and toggle Valgrind by
  exporting `USE_VALGRIND` before sourcing the non-`vg` script. Use
  `tests/timereported-utc-vg.sh` as the reference layout: it sources the base
  scenario instead of copying it and demonstrates paring back emitted messages
  when the underlying test is slow—especially important under Valgrind. Older
  wrappers still duplicate logic; prefer the modern pattern when touching them.
- Put auxiliary binaries next to their scripts (e.g. `*.c` programs compiled via
  the Automake harness) and list them in `tests/Makefile.am`.
- Keep long-lived configuration snippets in `tests/testsuites/` and reuse them
  instead of copying large config blocks into multiple scripts.
- Document new environment flags or helper functions inside `diag.sh` so other
  tests can discover them. Mention the addition in `tests/README` if operators
  should know about the new knob.

## Running tests locally
- Build rsyslog first (`./autogen.sh`, `./configure --enable-testbench`,
  `make -j$(nproc)`") so the testbench can load freshly built binaries and
  modules. The same bootstrap commands are called out in `tests/README` under
  “Quickstart”.
- Execute individual scenarios directly for quick feedback
  (`./tests/imfile-basic.sh`).
- Use `make check TESTS='script.sh'` when you need Automake logging,
  parallelisation control, or to exercise the Valgrind wrappers.
- Remove stale `.log`/`.trs` files before re-running a flaky test to avoid
  Automake caching previous outcomes.
- For configuration validation changes, run `./tests/validation-run.sh` to
  confirm both failure and success paths.

## Debugging & environment control
- `tests/diag.sh` documents environment variables such as `SUDO`,
  `USE_VALGRIND`, `RSYSLOG_DEBUG`, and timeout overrides; prefer these knobs over
  ad-hoc `sleep` loops. `tests/README` mirrors the operator-facing knobs so
  tooling and human docs stay aligned.
- Use `USE_GDB=1 make <test>.log` to pause execution and attach a debugger as
  described in `tests/README`.
- Keep suppression files (e.g. `*.supp`) current when adding new Valgrind noise;
  failing to do so will cause CI false positives.

## Coordination
- When adding tests for a plugin or runtime subsystem, mention them in the
  component’s `AGENTS.md` so future authors know smoke coverage exists.
- Update `KNOWN_ISSUES` or module metadata if a test encodes a known bug or a
  skipped scenario.
- If a change requires additional docker services or fixtures, document setup
  steps in `tests/CI/README` (or create it) and link from the relevant module
  guide.
