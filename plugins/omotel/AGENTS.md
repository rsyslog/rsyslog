# AGENTS.md – omotel output module

## Workflow & Skills

AI agents working on `omotel` MUST follow the standardized skills in `.agent/skills/`:

- **Build**: Use [`rsyslog_build`](../../.agent/skills/rsyslog_build/SKILL.md) (requires `--enable-omotel`).
- **Test**: Use [`rsyslog_test`](../../.agent/skills/rsyslog_test/SKILL.md).
  - **Policy**: Run `tests/omotel-http-batch.sh` to exercise the HTTP batching path.
- **Doc**: Use [`rsyslog_doc`](../../.agent/skills/rsyslog_doc/SKILL.md).
- **Module**: Use [`rsyslog_module`](../../.agent/skills/rsyslog_module/SKILL.md) and keep `MODULE_METADATA.yaml` current.

## Module Specifics

- **Dependencies**: Keep the module pure C unless gRPC is enabled.
- **Normalization**: Run `devtools/format-code.sh` before committing (see `rsyslog_commit`).
