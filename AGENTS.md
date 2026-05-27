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
| [`rsyslog_changelog`](.agent/skills/rsyslog_changelog/SKILL.md) | Selective ChangeLog maintenance that follows release-note style and avoids low-signal churn. |
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
4.  **Local AI Review**: Run local Cubic review when `cubic` is available.
5.  **Commit**: Use the `rsyslog_commit` skill to format code and draft your message.

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
- Full local container validation means the skill's ordered full sequence,
  including the static analyzer and Ubuntu 26.04 `run-ci.sh` check run. Focused
  container tests are useful targeted evidence, but they are not the full gate
  unless the skill explicitly allows the reduced lane for the touched area.
- Use the skill's configured CI-equivalent dev image, including Docker Hub dev
  images when appropriate. Use a locally built image only when validating that
  local image or the runtime container produced by the task.
- Run local Cubic validation when `cubic` is installed and reachable. Hosted
  Cubic or Gemini PR comments are additional review feedback, not substitutes
  for local Cubic or local container validation.
- Relax expensive or service-backed lanes only for the narrow touched-area
  cases documented in the container-testing skill, and record the rationale.
- If Docker or Podman is not installed, not running, lacks required
  permissions, or the required image cannot be obtained, state that exact
  blocker in the final response.
- If full local container validation is skipped or blocked, list the targeted
  validation that was run instead and explicitly mark the work as **not fully
  container-validated**.
- Do not describe implementation work as fully validated or complete unless
  full local container validation passed, or the user explicitly accepted the
  reduced validation scope after the blocker was reported.
- Session ledgers and final summaries for PR work must distinguish fully
  container-validated work from targeted container-tested-only work. Include the
  local Cubic status, hosted AI review status, image tag and ID, exact commands,
  lane relaxations, and pass/fail results.

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


## Python Style Validation

- Python style is governed by `setup.cfg` with `pycodestyle` line length set
  to 120 columns.
- For Python edits, run `devtools/format-python.sh <changed-python-files>`
  when `pycodestyle` is installed. Use `devtools/format-python.sh --fix
  <changed-python-files>` to run `autopep8` first.
- If `pycodestyle` or `autopep8` is not installed in a local agent environment,
  suggest installing it (`sudo apt-get install -y pycodestyle
  python3-autopep8` on Debian/Ubuntu) but do not block unrelated build or
  test validation. Agents may use `devtools/format-python.sh --check-if-available ...` for
  optional local checks.
- The GitHub Actions `python_style.yml` workflow installs `pycodestyle` and
  checks only changed Python files in pull requests. It does not run `autopep8`.
  Do not introduce full-tree Python style gates unless the baseline is
  intentionally refreshed in the same change.
- Be cautious with legacy Python-2-style helper scripts: review any `autopep8`
  changes that touch print statements, exception syntax, imports, or line
  continuations.

## Optional Local Linter Passes

CodeFactor and CI provide centralized lint feedback, but agents SHOULD run
useful local linters on the PR diff when the tools are already installed. These
checks are advisory local validation: if a tool is missing, suggest installing
it and continue with the normal build/test flow.

Use a freshly fetched upstream base when computing changed files:

```bash
git fetch upstream main --prune
```

- For changed shell scripts, run `shellcheck` when installed:
  `command -v shellcheck >/dev/null && git diff -z --name-only
  --diff-filter=ACMR upstream/main...HEAD -- '*.sh' | xargs -0 -r
  shellcheck -S warning`
- For changed Dockerfiles, run `hadolint` when installed:
  `command -v hadolint >/dev/null && git diff -z --name-only
  --diff-filter=ACMR upstream/main...HEAD -- '*Dockerfile*' 'Dockerfile' |
  xargs -0 -r hadolint`
- For changed infrastructure/config files, run `trivy config` when installed.
  Prefer changed paths or the smallest relevant directory over a full-repo scan.
- For larger PRs, run `jscpd` on changed source/test files when installed to
  catch accidental copy/paste duplication. Treat findings as review prompts,
  not automatic blockers.

Do not add `cppcheck` to the routine local PR checklist for this repository
unless a maintainer explicitly asks for it; it has historically produced too
much low-value noise on the rsyslog code base.

## GitHub Actions Validation

- When editing files under `.github/workflows/`, validate locally with
  `actionlint .github/workflows/<file>.yml` and the pinned zizmor version:
  `python3 -m venv .zizmor-venv && .zizmor-venv/bin/python -m pip install -r .github/requirements-zizmor.txt && .zizmor-venv/bin/zizmor --strict-collection .github/workflows`.
- Avoid direct `${{ ... }}` template expansion inside shell `run:` scripts.
  Pass expression values through `env:` variables and expand those variables in
  the shell script instead.

## PR Test Relevance Policy

- Regular pull-request CI may use approximate relevance gates to avoid
  scheduling expensive service-backed test families that cannot reasonably be
  affected by the change. The goal is to omit irrelevant tests from the
  configured Automake `TESTS` set, not merely to start those tests and skip
  them late after service setup.
- Relevance gates must be conservative. Direct module changes, related tests,
  testbench/build plumbing, workflow files, configure inputs, and shared runtime
  paths that can plausibly affect the service family must keep that family
  enabled.
- Isolated helper areas may be excluded from a heavy family only when there is a
  clear rationale that the family cannot use that code path. Current examples
  include keeping Kafka, imfile, and Elasticsearch tests disabled for unrelated
  helper-only changes such as lookup tables or dynstats.
- Agents changing relevance rules must validate both levels:
  `tests/diag.sh module-needs-testing <family>` with representative changed-file
  sets, and a container/mock CI run that confirms the generated test list omits
  irrelevant heavy-family tests before execution.
- Full coverage must remain forceable. Daily, weekly, release, flake-campaign,
  and maintainer-requested runs must be able to bypass relevance gates via
  `RSYSLOG_TESTBENCH_FORCE_SERVICE_TESTS=1`,
  `RSYSLOG_TESTBENCH_FORCE_<FAMILY>_TESTS=1`, or
  `RSYSLOG_TESTBENCH_SKIP_SERVICE_RELEVANCE=1`.
- Do not present a relevance-filtered PR run as equivalent to an unconditional
  full-suite run. Report which families were disabled and why when that matters
  for the validation claim.

## Agent Chat Keywords

- `SETUP`: Triggers the `rsyslog_build` setup workflow.
- `BUILD`: Triggers the `rsyslog_build` incremental build workflow.
- `TEST`: Triggers the `rsyslog_test` validation workflow.
- `CHANGELOG`: Triggers the `rsyslog_changelog` release-note maintenance workflow.
- `SUMMARIZE`: Generates PR and commit summaries using `rsyslog_commit` templates.
- `FINISH`: Final review of code and style before conclusion.

---
*For human-facing guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md) and [DEVELOPING.md](DEVELOPING.md).*
