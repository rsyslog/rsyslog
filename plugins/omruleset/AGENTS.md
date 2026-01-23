# AGENTS.md – omruleset compatibility module

## Workflow & Skills

AI agents working on `omruleset` MUST follow the standardized skills in `.agent/skills/`:

- **Build**: Use [`rsyslog_build`](../../.agent/skills/rsyslog_build/SKILL.md).
- **Test**: Use [`rsyslog_test`](../../.agent/skills/rsyslog_test/SKILL.md).
- **Module**: Use [`rsyslog_module`](../../.agent/skills/rsyslog_module/SKILL.md).

## Module Specifics

- **Status**: Maturity is **deprecated**. Replaced by RainerScript `call` statement.
- **Testing**: No standalone suite; rely on configuration-level tests.
- **Coordination**: Update migration notes in documentation when fixing compatibility issues.
