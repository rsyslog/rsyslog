# AGENTS.md – Documentation subtree

This guide applies to everything under `doc/`.

## Workflow & Skills

AI agents working on documentation MUST use the standardized skills in `.agent/skills/`:

- **Doc**: Use [`rsyslog_doc`](../.agent/skills/rsyslog_doc/SKILL.md) for metadata blocks, summary slices, and Sphinx validation.
- **Dist**: Use [`rsyslog_doc_dist`](../.agent/skills/rsyslog_doc_dist/SKILL.md) for `doc/Makefile.am` synchronization.
- **Commit**: Use [`rsyslog_commit`](../.agent/skills/rsyslog_commit/SKILL.md) for doc-specific commit trailers.

## Subtree Specifics

- **Knowledge Base**: `doc/ai/` contains canonical guides for Mermaid, Terminology, and authoring.
- **Prompting**: Use `ai/rsyslog_code_doc_builder/base_prompt.txt` for tone and style guidance.
- **Build**: Use `./doc/tools/build-doc-linux.sh --clean --format html` for validation.

---
*For detailed authoring rules, see [doc/ai/AGENTS.md](./ai/AGENTS.md).*
