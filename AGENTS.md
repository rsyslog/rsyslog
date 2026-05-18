# AGENTS.md – rsyslog Repository Agent Guide

This file defines the high-level roadmap for AI assistants to understand and contribute to the rsyslog codebase. Technical workflows are now modularized into **Skills**.

## Local Overlay

Before starting work in this repository, read `AGENTS.local.md` if it exists.
That file contains machine- and workflow-specific instructions that are not
duplicated here.

## AI Agent Skills

To ensure consistency and high-quality contributions, AI agents SHOULD use the following standardized skills located in `.agent/skills/`:

| Skill | Purpose |
|-------|---------|
| [`rsyslog_build`](.agent/skills/rsyslog_build/SKILL.md) | Environment setup and incremental parallel builds. |
| [`rsyslog_test`](.agent/skills/rsyslog_test/SKILL.md) | Standardized validation and debugging via `diag.sh`. |
| [`rsyslog_local_container_testing`](.agent/skills/rsyslog_local_container_testing/SKILL.md) | CI-style local dev-container validation, analyzer-first flow, service-skip checks, and clean-tree rules. |
| [`rsyslog_pr_babysitting`](.agent/skills/rsyslog_pr_babysitting/SKILL.md) | Post-push PR monitoring, including CI failures, reruns, and unresolved review-thread checks. |
| [`rsyslog_doc`](.agent/skills/rsyslog_doc/SKILL.md) | Structured, RAG-optimized documentation and metadata. |
| [`rsyslog_doc_dist`](.agent/skills/rsyslog_doc_dist/SKILL.md) | Syncing documentation files in `doc/Makefile.am`. |
| [`rsyslog_module`](.agent/skills/rsyslog_module/SKILL.md) | Technical patterns for concurrency and module authoring. |
| [`rsyslog_config`](.agent/skills/rsyslog_config/SKILL.md) | Dual-frontend config architecture (RainerScript + YAML) and parity rules. |
| [`rsyslog_issue_triage`](.agent/skills/rsyslog_issue_triage/SKILL.md) | GitHub issue backlog triage, clustering, closure comments, and local evidence boards. |
| [`rsyslog_commit`](.agent/skills/rsyslog_commit/SKILL.md) | Compliant commit messages and branching policies. |

## Agent Quick Start: The "Happy Path"

Follow these steps for a typical development task:

1.  **Build**: Use the `rsyslog_build` skill to set up and compile.
2.  **Validate**: Use the `rsyslog_test` skill to run relevant shell tests.
3.  **Container Validation**: Use the `rsyslog_local_container_testing` skill
    when Docker or Podman container tooling is available.
4.  **Commit**: Use the `rsyslog_commit` skill to format code and draft your message.

Tip: You do NOT need to re-run your build, test, or container validation cycle
after formatting if you already validated the code immediately before.

## Repository Overview

- **Primary Language**: C (v8 worker model)
- **Architecture**: Microkernel core (`runtime/`) + Loadable Plugins (`plugins/`)
- **Metadata**: Every module directory contains `MODULE_METADATA.yaml`.
- **Knowledge Base**: `doc/ai/` contains canonical patterns for RAG ingestion.
- **Security Triage**: [`doc/ai/security_triage_rubric.md`](./doc/ai/security_triage_rubric.md)
  defines how AI agents must distinguish confirmed issues from potential
  issues, hardening, and invalid findings before using security severity or CWE
  language.

## Container Images

- Runtime container definitions live in `packaging/docker/rsyslog`.
- Local GitHub Actions-style validation commands for the Ubuntu 26.04 dev
  container, `-j80` check runs, clang static analyzer, disabled external
  services, and Docker storage cleanup are documented in the
  [`rsyslog_local_container_testing`](.agent/skills/rsyslog_local_container_testing/SKILL.md)
  skill. AI agents should use that skill when running or planning this
  validation.
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

## Required Final Validation Gate

For implementation tasks, AI agents MUST treat full local container validation
as the final validation gate when container tooling is available.

- If Docker or Podman is available and usable, run the
  [`rsyslog_local_container_testing`](.agent/skills/rsyslog_local_container_testing/SKILL.md)
  skill's full local validation before reporting the task complete.
- If Docker or Podman is not installed, not running, lacks required
  permissions, or the required image cannot be obtained, state that exact
  blocker in the final response.
- If full local container validation is skipped or blocked, list the targeted
  validation that was run instead and explicitly mark the work as **not fully
  container-validated**.
- Do not describe implementation work as fully validated or complete unless
  full local container validation passed, or the user explicitly accepted the
  reduced validation scope after the blocker was reported.

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
- New and changed tests must include inline intent documentation that says what
  behavior, regression, or invariant they test. If an existing test lacks that
  context, add it while touching the test.

  For timing, retry, sampling, concurrency, or negative-path tests, also explain
  the oracle: what proves success or failure, and why any wait or threshold
  exists.

  When changing a test, verify that the head comment still matches the actual
  setup, stimulus, oracle, and pass/fail conditions after the edit; update it in
  the same commit if it does not.
- It is fine to organize sources under `tests/unit/`, `tests/helpers/`, or
  similar folders, but register and run those tests from `tests/Makefile.am`.
- Do not introduce additional recursive `tests/.../Makefile.am` test harnesses.
  Top-level `make check TESTS=...` propagates into every subdirectory, and
  multiple test-owning subdirs make targeted selection fragile.

## GitHub Actions Validation

- When editing files under `.github/workflows/`, validate locally with
  `actionlint .github/workflows/<file>.yml` and the pinned zizmor version:
  `python3 -m venv .zizmor-venv && .zizmor-venv/bin/python -m pip install -r .github/requirements-zizmor.txt && .zizmor-venv/bin/zizmor --strict-collection .github/workflows`.
- Avoid direct `${{ ... }}` template expansion inside shell `run:` scripts.
  Pass expression values through `env:` variables and expand those variables in
  the shell script instead.

## Agent Chat Keywords

- `SETUP`: Triggers the `rsyslog_build` setup workflow.
- `BUILD`: Triggers the `rsyslog_build` incremental build workflow.
- `TEST`: Triggers the `rsyslog_test` validation workflow.
- `SUMMARIZE`: Generates PR and commit summaries using `rsyslog_commit` templates.
- `FINISH`: Final review of code and style before conclusion.

---
*For human-facing guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md) and [DEVELOPING.md](DEVELOPING.md).*
