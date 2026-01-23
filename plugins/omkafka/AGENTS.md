# AGENTS.md – omkafka output module

## Workflow & Skills

AI agents working on `omkafka` MUST follow the standardized skills in `.agent/skills/`:

- **Build**: Use [`rsyslog_build`](../../.agent/skills/rsyslog_build/SKILL.md) (requires `--enable-omkafka`).
- **Test**: Use [`rsyslog_test`](../../.agent/skills/rsyslog_test/SKILL.md).
  - **Policy**: Skip heavy integration tests (e.g., `./tests/omkafka.sh`) in sandboxes as they require a local Kafka cluster.
- **Doc**: Use [`rsyslog_doc`](../../.agent/skills/rsyslog_doc/SKILL.md).
- **Module**: Use [`rsyslog_module`](../../.agent/skills/rsyslog_module/SKILL.md) and keep `MODULE_METADATA.yaml` current.

## Module Specifics

- **Dependencies**: Ships events via `librdkafka`.
- **Diagnostics**: `impstats` exposes counters (submitted, failed, retry).
- **Review**: Changes to shared Kafka helpers in `runtime/` must be coordinated with `imkafka`.
