# AGENTS.md – Contrib modules subtree

These instructions apply to everything under `contrib/`.

## Workflow & Skills

AI agents working in `contrib/` MUST follow the standardized skills in `.agent/skills/`:

- **Build**: Use [`rsyslog_build`](../.agent/skills/rsyslog_build/SKILL.md) for incremental parallel builds.
- **Test**: Use [`rsyslog_test`](../.agent/skills/rsyslog_test/SKILL.md) for `diag.sh` based validation.
- **Doc**: Use [`rsyslog_doc`](../.agent/skills/rsyslog_doc/SKILL.md) for metadata and documentation.
- **Module**: Use [`rsyslog_module`](../.agent/skills/rsyslog_module/SKILL.md) for `MODULE_METADATA.yaml` requirements.

## Contrib Expectations

- Changes SHOULD preserve backward compatibility.
- Use `MODULE_METADATA.yaml` to document third-party dependencies and manual setup.

---
*For human-facing guidelines, see [CONTRIBUTING.md](../CONTRIBUTING.md).*
