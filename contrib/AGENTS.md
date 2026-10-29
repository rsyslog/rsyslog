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

## Regression coverage

- `imbatchreport`: `tests/imbatchreport-multiple-inputs.sh` and its native
  YAML wrapper configure three distinct report globs with three files each.
  Exact output and consumed-file checks cover retention of every input
  instance; the existing single-input tests remain controls.

---
*For human-facing guidelines, see [CONTRIBUTING.md](../CONTRIBUTING.md).*
