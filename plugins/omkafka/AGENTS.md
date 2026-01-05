# AGENTS.md – omkafka output module

## Module overview
- Ships events to Apache Kafka topics via librdkafka.
- User documentation: `doc/source/configuration/modules/omkafka.rst`.
- Support status: contributor-supported. Maturity: mature.

## Build & dependencies
- **Efficient Build:** Use `make -j$(nproc) check TESTS=""` to build the module and test dependencies.
- **Configure:** Run `./configure --enable-omkafka` (and `--enable-imkafka` if needed).
- **Bootstrap:** Only run `./autogen.sh` if you touch `configure.ac`, `Makefile.am`, or `m4/`.

## Local testing
- **Skip the Kafka integration tests for routine agent tasks.** They download and run Kafka plus ZooKeeper, which exceeds the sandbox resource budget.
- Build validation is sufficient. Run the efficient build command above.
- Maintainers who must exercise the suite can enable `--enable-kafka-tests` and run scripts such as `./tests/omkafka.sh`, but expect multi-minute startup time for the embedded Kafka cluster.

## Diagnostics & troubleshooting
- `impstats` exposes the `omkafka` counter set (submitted, failed, retry metrics); enable the module and inspect `impstats` output for delivery issues.
- Kafka-side diagnostics live in the working directory under `.dep_wrk/`; the helper `./tests/diag.sh dump-kafka-topic <topic>` extracts queued messages for debugging.

## Cross-component coordination
- Changes to shared Kafka helpers in `runtime/` or `tests/diag.sh` must also be reviewed by `imkafka` maintainers.
- Align parameter documentation with `doc/source/configuration/modules/omkafka.rst` and update examples when defaults change.

## Metadata & housekeeping
- Keep `plugins/omkafka/MODULE_METADATA.yaml` current (support status, maturity, contacts).
- Update `doc/ai/module_map.yaml` if the concurrency model or locking guidance changes.
