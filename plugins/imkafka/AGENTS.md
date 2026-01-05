# AGENTS.md – imkafka input module

## Module overview
- Consumes events from Apache Kafka topics via librdkafka.
- User documentation: `doc/source/configuration/modules/imkafka.rst`.
- Support status: contributor-supported. Maturity: mature.

## Build & dependencies
- **Efficient Build:** Use `make -j$(nproc) check TESTS=""` to build the module and test dependencies.
- **Configure:** Run `./configure --enable-imkafka` (and `--enable-omkafka` if needed).
- **Bootstrap:** Only run `./autogen.sh` if you touch `configure.ac`, `Makefile.am`, or `m4/`.

## Local testing
- **Skip the Kafka integration tests when working in the sandbox.** They rely on downloading and running Kafka plus ZooKeeper, which is too resource intensive for agent runs.
- Build-only validation is acceptable. Run the efficient build command above.
- Maintainers can enable `--enable-kafka-tests` and invoke scripts like `./tests/imkafka.sh` or `./tests/imkafka_multi_group.sh`, but should expect several minutes of setup time for the Kafka cluster.

## Diagnostics & troubleshooting
- `impstats` exposes the `imkafka` counter set (submitted, failed, and lag metrics). Monitor it to confirm offsets advance as expected.
- The helper `./tests/diag.sh dump-kafka-topic <topic>` retrieves topic contents when diagnosing ingestion issues.

## Cross-component coordination
- Coordinate changes with `omkafka` to keep shared Kafka helpers, configuration parameters, and documentation aligned.
- Update `doc/source/configuration/modules/imkafka.rst` whenever defaults or supported features change.

## Metadata & housekeeping
- Keep `plugins/imkafka/MODULE_METADATA.yaml` accurate and refresh `last_reviewed` when updating ownership or support status.
- Update `doc/ai/module_map.yaml` if concurrency or state-handling behavior changes.
