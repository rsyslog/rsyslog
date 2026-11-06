# Fuzz harness migration report

## Scope

This change keeps the existing rsyslog parser fuzz scenarios while moving the
drivers, runtime fixtures, corpus, dictionaries, replay helpers, and CI entry
points into the self-contained `fuzz/` subtree. Product parser, message,
queue, template, and runtime implementations remain in their existing rsyslog
subtrees and are linked into the harness build.

## Harness fixes

| Problem | Change | Regression |
| --- | --- | --- |
| Initialization failure became a successful no-op | Both drivers fail before dispatch when `rs_fuzz_init()` fails | `fuzz_driver_regression` |
| Zero-length input was discarded by the driver | The shared dispatcher calls the parser with logical length zero | `fuzz_driver_regression`; `empty_test.txt` |
| Legacy configuration leaked between testcases | Owned configuration state is snapshotted and restored per dispatch | `fuzz_state_regression aa`; `fuzz_state_regression aba` |
| Temporary imfile-like input ignored `TMPDIR` | Temporary paths are based on `TMPDIR`, with `/tmp` only as fallback | File and libFuzzer replay with an isolated `TMPDIR` |
| AFL-instrumented file mode dispatched once before entering `__AFL_LOOP` | AFL builds let the persistent loop exclusively control testcase dispatch; ordinary file mode still runs once | `check-fuzz-regressions` and bounded AFL++ smoke |
| Fuzz-only configure required unused optional dependencies | The CI configure disables unused libyaml and impstats push support | Alpine CI build |
| Coverage growth prevented `AFL_EXIT_ON_TIME` from bounding the campaign | The CI entry point passes `FUZZ_MAX_TOTAL_TIME_SECONDS` to AFL++ `-V` | Bounded AFL++ smoke |

Validation results belong in the pull request or CI artifacts rather than this
distributed source document. The durable commands for building, replaying,
minimizing, and triaging the harnesses are documented in `fuzz/README.md`.
