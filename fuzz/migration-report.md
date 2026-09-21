# Fuzz harness migration report

## Scope

This change keeps the existing rsyslog parser fuzz scenarios while moving the
drivers, runtime fixtures, corpus, dictionaries, replay helpers, and CI entry
points into the self-contained `fuzz/` subtree. Product parser, message,
queue, template, and runtime implementations remain in their existing rsyslog
subtrees and are linked into the harness build.

Tested rsyslog revision:
`c5cafe3e85679be9e6c3434c9ac23399eab32b9d`.

## Harness fixes

| Problem | Change | Regression | Result |
| --- | --- | --- | --- |
| Initialization failure became a successful no-op | Both drivers fail before dispatch when `rs_fuzz_init()` fails | `fuzz_driver_regression` | PASS |
| Zero-length input was discarded by the driver | The shared dispatcher calls the parser with logical length zero | `fuzz_driver_regression`; `empty_test.txt` | PASS |
| Legacy configuration leaked between testcases | Owned configuration state is snapshotted and restored per dispatch | `fuzz_state_regression aa`; `fuzz_state_regression aba` | PASS |
| Temporary imfile-like input ignored `TMPDIR` | Temporary paths are based on `TMPDIR`, with `/tmp` only as fallback | file/libFuzzer replay with an isolated `TMPDIR` | PASS |
| AFL-instrumented file mode dispatched once before entering `__AFL_LOOP` | AFL builds now let the persistent loop exclusively control testcase dispatch; ordinary file mode still runs once | Alpine `check-fuzz-regressions` and bounded AFL++ smoke | PASS |
| CI configure required optional Alpine dependencies | The fuzz-only configure disables unused libyaml and impstats push support | Alpine CI build | PASS |
| `AFL_EXIT_ON_TIME` did not impose a hard campaign limit while coverage grew | The CI entry point also passes the configurable `FUZZ_MAX_TOTAL_TIME_SECONDS` value to AFL++ `-V` | Five-second Alpine AFL++ smoke | PASS |

## Current validation

| Check | Command | Result | Status |
| --- | --- | --- | --- |
| Clang sanitizer build | `make -C fuzz ... fuzz_rsyslog_parsers_libfuzzer` | Binary built with ASan/UBSan | PASS |
| Harness regressions | `make -C fuzz ... check-fuzz-regressions` | Initialization, empty, A-to-A, A-to-B-to-A passed | PASS |
| libFuzzer corpus replay | `FUZZ_TIMEOUT=15 fuzz/run_fuzz_smoke.sh libfuzzer` | 50 of 50 files processed | PASS |
| Empty input | libFuzzer `-runs=1 empty_test.txt` | Zero-byte file executed | PASS |
| Short input | libFuzzer `-runs=1 short_timestamp_2.txt` | Two-byte file executed | PASS |
| Binary input | libFuzzer `-runs=1 binary_data_bytes.txt` | 256-byte file executed | PASS |
| Repeated libFuzzer calls | Four existing files passed to one process | All four executed | PASS |
| ASan/UBSan diagnostics | Scan of build, regression, and replay logs | No diagnostics | PASS |
| AFL++ build/smoke | `afl-clang-fast` build; bounded `afl-fuzz -V 5` | Persistent/deferred forkserver detected; 49 non-empty seeds loaded; 0 crashes and 0 timeouts | PASS |
| Distribution build | `make distcheck TEST_RUN_TYPE=MOCK-OK -j2` | `rsyslog-8.2610.0.daily.tar.gz` produced | PASS |
| Alpine image | `docker build -f fuzz/Dockerfile -t rsyslog-fuzz-pr:local .` | Alpine 3.23 image with AFL++ and CASR 2.13.0 built | PASS |
| Alpine CI entry point | `FUZZ_MAX_TOTAL_TIME_SECONDS=5 ... /bin/sh fuzz/ci_build_and_fuzz.sh` | Build, regressions, 50-file replay, dictionaries, and five-second AFL++ smoke passed | PASS |
| Upstream static analyzer | Ubuntu 26.04 `devtools/run-static-analyzer.sh` with Clang | `scan-build: No bugs found` | PASS |
| Upstream change-gated testbench | Not run because the agreed validation scope excludes real network sockets | Covered locally by focused fuzz regressions, replay, smoke, build, and distcheck only | NOT_RUN |
| GitLab CI syntax | Ruby YAML parse plus shell `sh -n` | Two stages and entry-point scripts parse locally | PASS |
| Remote GitLab pipeline | Not run from the local checkout | Requires pushed branch and GitLab runner | NOT_RUN |

The separately excluded runtime probes remain NOT_RUN at the user's request;
they are not represented as PASS by the regression results above.
