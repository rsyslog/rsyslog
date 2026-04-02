# AGENTS.md – rsyslog Repository Agent Guide

This file defines the high-level roadmap for AI assistants to understand and contribute to the rsyslog codebase. Technical workflows are now modularized into **Skills**.

## AI Agent Skills

To ensure consistency and high-quality contributions, AI agents SHOULD use the following standardized skills located in `.agent/skills/`:

| Skill | Purpose |
|-------|---------|
| [`rsyslog_build`](.agent/skills/rsyslog_build/SKILL.md) | Environment setup and incremental parallel builds. |
| [`rsyslog_test`](.agent/skills/rsyslog_test/SKILL.md) | Standardized validation and debugging via `diag.sh`. |
| [`rsyslog_doc`](.agent/skills/rsyslog_doc/SKILL.md) | Structured, RAG-optimized documentation and metadata. |
| [`rsyslog_doc_dist`](.agent/skills/rsyslog_doc_dist/SKILL.md) | Syncing documentation files in `doc/Makefile.am`. |
| [`rsyslog_module`](.agent/skills/rsyslog_module/SKILL.md) | Technical patterns for concurrency and module authoring. |
| [`rsyslog_config`](.agent/skills/rsyslog_config/SKILL.md) | Dual-frontend config architecture (RainerScript + YAML) and parity rules. |
| [`rsyslog_commit`](.agent/skills/rsyslog_commit/SKILL.md) | Compliant commit messages and branching policies. |

## Agent Quick Start: The "Happy Path"

Follow these three steps for a typical development task:

1.  **Build**: Use the `rsyslog_build` skill to set up and compile.
2.  **Validate**: Use the `rsyslog_test` skill to run relevant shell tests.
3.  **Commit**: Use the `rsyslog_commit` skill to format code and draft your message.
    - *Tip*: You do NOT need to re-run your build/test cycle after formatting if you already validated the code immediately before.

## Repository Overview

- **Primary Language**: C (v8 worker model)
- **Architecture**: Microkernel core (`runtime/`) + Loadable Plugins (`plugins/`)
- **Metadata**: Every module directory contains `MODULE_METADATA.yaml`.
- **Knowledge Base**: `doc/ai/` contains canonical patterns for RAG ingestion.

## Container Images

- Runtime container definitions live in `packaging/docker/rsyslog`.
- The container Makefile default version must stay clearly non-release.
  Use explicit `VERSION=...` values for release-like local rehearsals and for
  any publish automation.
- Release-tagged container images are downstream of package publishing.
  AI agents must not add or use release container flows that bypass the
  Adiscon PPA readiness check.
- Manual release flows use two fixed channels:
  `stable` maps `8.yymm.0` to `20yy-mm` via `ppa:adiscon/v8-stable`,
  and `daily-stable` uses `ppa:adiscon/daily-stable` with the fixed tag
  `daily-stable`.
- AI agents must not introduce release-looking fallback tags such as
  `2026-03` as the default local container build version.

## Context Discovery (Subtree Guides)

Each major subtree contains a specialized `AGENTS.md` that points to area-specific context and requirements:

- **Documentation**: [`doc/AGENTS.md`](./doc/AGENTS.md)
- **Core Plugins**: [`plugins/AGENTS.md`](./plugins/AGENTS.md)
- **Contrib Modules**: [`contrib/AGENTS.md`](./contrib/AGENTS.md)
- **Runtime Core**: [`runtime/AGENTS.md`](./runtime/AGENTS.md)
- **Testbench**: [`tests/AGENTS.md`](./tests/AGENTS.md)
- **Built-in Tools**: [`tools/AGENTS.md`](./tools/AGENTS.md)

## Test Structure Rule

- For this recursive Automake tree, keep `tests/` as the single recursive
  test-owning subtree.
- It is fine to organize sources under `tests/unit/`, `tests/helpers/`, or
  similar folders, but register and run those tests from `tests/Makefile.am`.
- Do not introduce additional recursive `tests/.../Makefile.am` test harnesses.
  Top-level `make check TESTS=...` propagates into every subdirectory, and
  multiple test-owning subdirs make targeted selection fragile.

## Agent Chat Keywords

- `SETUP`: Triggers the `rsyslog_build` setup workflow.
- `BUILD`: Triggers the `rsyslog_build` incremental build workflow.
- `TEST`: Triggers the `rsyslog_test` validation workflow.
- `SUMMARIZE`: Generates PR and commit summaries using `rsyslog_commit` templates.
- `FINISH`: Final review of code and style before conclusion.

---
*For human-facing guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md) and [DEVELOPING.md](DEVELOPING.md).*
