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
| [`rsyslog_local_container_testing`](.agent/skills/rsyslog_local_container_testing/SKILL.md) | CI-style local dev-container validation, change-gated Ubuntu 26.04 first, late prompt audits, service-skip checks, and clean-tree rules. |
| [`rsyslog_pr_babysitting`](.agent/skills/rsyslog_pr_babysitting/SKILL.md) | Post-push PR monitoring, including CI failures, reruns, and unresolved review-thread checks. |
| [`rsyslog_changelog`](.agent/skills/rsyslog_changelog/SKILL.md) | Selective ChangeLog maintenance that follows release-note style and avoids low-signal churn. |
| [`rsyslog_v8stable_patch_flow`](.agent/skills/rsyslog_v8stable_patch_flow/SKILL.md) | Post-.0 v8-stable patch updates, patch-release ChangeLog sections, and clean stable-to-main merges. |
| [`rsyslog_doc`](.agent/skills/rsyslog_doc/SKILL.md) | Structured, RAG-optimized documentation and metadata. |
| [`rsyslog_doc_dist`](.agent/skills/rsyslog_doc_dist/SKILL.md) | Syncing documentation files in `doc/Makefile.am`. |
| [`rsyslog_module`](.agent/skills/rsyslog_module/SKILL.md) | Technical patterns for concurrency and module authoring. |
| [`rsyslog_config`](.agent/skills/rsyslog_config/SKILL.md) | Dual-frontend config architecture (RainerScript + YAML) and parity rules. |
| [`rsyslog_issue_triage`](.agent/skills/rsyslog_issue_triage/SKILL.md) | GitHub issue backlog triage, clustering, closure comments, and local evidence boards. |
| [`rsyslog_continuous_issue_session`](.agent/skills/rsyslog_continuous_issue_session/SKILL.md) | Long-running issue-fix sessions with a rolling active PR set, validation gates, babysitting, cleanup, and automatic refill from the local issue cache. |
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
  container, local concurrency knobs, clang static analyzer, disabled external
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

For implementation tasks, AI agents MUST treat PR-ready local container
validation as the final validation gate when container tooling is available.

- Start by running `devtools/local-validation-plan.sh` to classify the local
  diff. Use `devtools/local-validation-plan.sh --run` when you want the helper
  to execute the selected validation path and fail on the first required error.
  The helper includes committed branch changes, staged/unstaged tracked changes,
  and untracked files, so it is safer than manually inspecting only
  `origin/main...HEAD` during active local work.
- Agent-documentation and skill-only edits, limited to files such as
  `AGENTS.md`, `AGENTS.local.md`, `.agent/skills/**`, or `.codex/skills/**`,
  do not require a full local CI container run or static analyzer. Validate
  them with text review, targeted command-snippet checks when useful, and the
  relevant documentation/style checks. If the same change also touches code,
  tests, workflows, build files, or scripts, validate those touched areas via
  the container-testing skill.
- User-facing documentation edits that affect rendered Sphinx docs under
  `doc/source/**`, or Sphinx support files for that tree, should run a
  high-concurrency docs build instead of local runtime CI:
  `./doc/tools/build-doc-linux.sh --clean --format html --jobs "${RSYSLOG_LOCAL_DOC_JOBS:-$(nproc)}"`.
  Internal agent, skill, and AI-maintenance docs that are not rendered into the
  user manual do not require this docs build unless they also change rendered
  Sphinx inputs.
- If Docker or Podman is available and usable, run the
  [`rsyslog_local_container_testing`](.agent/skills/rsyslog_local_container_testing/SKILL.md)
  skill's PR-ready local validation before reporting the task complete.
- PR-ready local container validation means the skill's ordered
  change-gated sequence: the Ubuntu 26.04 `run-ci.sh` check run using the same
  relevance gates as regular PR CI, the Ubuntu 26.04 static analyzer where
  applicable, late prompt-based audit passes where applicable, and local Cubic
  where applicable. Focused container tests are useful targeted evidence, but
  they are not the final gate unless the skill explicitly allows the reduced
  lane for the touched area.
- Use the skill's configured CI-equivalent dev image, including Docker Hub dev
  images when appropriate. Use a locally built image only when validating that
  local image or the runtime container produced by the task.
- Run local Cubic validation for code changes when `cubic` is installed and
  reachable. Do not run Cubic for documentation-only changes. For tests,
  workflow, build, and other non-code changes, use Cubic when the change is
  non-trivial, behavior-affecting, security-sensitive, or large. Hosted Cubic
  or Gemini PR comments are additional review feedback, not substitutes for
  local Cubic or local container validation.
- Agents should honor local machine capacity knobs when running broad local
  checks: `RSYSLOG_LOCAL_CHECK_JOBS` for `make check` concurrency and
  `RSYSLOG_LOCAL_BUILD_JOBS` for build concurrency, both defaulting to `10`
  when unset. Deflake or overload experiments must use explicit prompt-provided
  `-jN` values instead of changing these defaults.
- Relax expensive or service-backed lanes only for the narrow touched-area
  cases documented in the container-testing skill, and record the rationale.
- If Docker or Podman is not installed, not running, lacks required
  permissions, or the required image cannot be obtained, state that exact
  blocker in the final response.
- If PR-ready local container validation is skipped or blocked, list the
  targeted validation that was run instead and explicitly mark the work as
  **not fully container-validated**.
- Do not describe implementation work as fully validated or complete unless
  PR-ready local container validation passed, or the user explicitly accepted
  the reduced validation scope after the blocker was reported.
- Session ledgers and final summaries for PR work must distinguish fully
  PR-ready container-validated work from targeted container-tested-only work.
  Include the local Cubic status, hosted AI review status, image tag and ID,
  exact commands, local concurrency values, lane relaxations, and pass/fail
  results.

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

## C Formatting Validation

- C style is governed by `.clang-format` and the exact formatter configured in
  `devtools/format-code.sh` (`clang-format-18` at the time of writing).
- For C or header edits, agents MUST run
  `devtools/format-code.sh --git-changed` before committing when local files
  may need formatting.
- For deterministic local validation, agents MUST use the read-only gate:
  `devtools/format-code.sh --git-changed --check --check-if-available`.
  This runs `clang-format` in dry-run mode and fails if changed C/H files would
  be rewritten. If the exact formatter is missing, it warns and leaves hosted CI
  or a fuller local environment to cover the gap.
- CI will not pass with improperly formatted C/H code. A missing local
  formatter is only a local tooling limitation, not permission to skip
  formatting.
- `devtools/local-validation-plan.sh --run` executes this read-only C format
  check automatically for changed C/H files before heavier validation.

## Local Preflight Linters

CodeFactor and CI provide centralized lint feedback, but agents SHOULD run
useful local linters on the PR diff before heavier validation when the tools are
already installed. These checks are advisory local validation: if a tool is
missing, suggest installing it and continue with the normal build/test flow.

Use a freshly fetched upstream base when computing changed files:

```bash
git fetch upstream main --prune
```

- For changed shell scripts, run `shellcheck` when installed:
  `command -v shellcheck >/dev/null && git diff -z --name-only
  --diff-filter=ACMR upstream/main...HEAD -- '*.sh' | xargs -0 -r
  shellcheck -S warning`
- For changed scripts with a POSIX `sh` shebang, run `checkbashisms` when
  installed. On Debian/Ubuntu it is provided by `devscripts`
  (`sudo apt-get install -y devscripts`). Do not use full-tree
  `checkbashisms` as a routine gate for the testbench; many tests intentionally
  use Bash-oriented testbench conventions. Prefer checking changed files that
  claim `/bin/sh` portability so runtime stays proportional to the PR delta:
  `command -v checkbashisms >/dev/null && git diff --name-only
  --diff-filter=ACMR upstream/main...HEAD -- '*.sh' | while IFS= read -r f;
  do case "$(head -n1 "$f")" in '#!/bin/sh'|'#!/usr/bin/sh'|'#!/usr/bin/env sh')
  checkbashisms -p "$f";; esac; done`
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

## Limited Local Environments

Agents, workstations, and external contributor environments without Docker or
Podman access, including Web Agents, must still do the best deterministic
validation available in their current configuration. We must support those
environments: missing local tools are coverage gaps, not local workflow
blockers. Run every applicable diff-scoped linter, syntax check, static check,
documentation build, host build, or focused host-side test that is available
and relevant to the change. Inspect the CI configuration and changed files, and
identify the hosted or local-container lanes that should cover anything the
current environment cannot run. Do not claim PR-ready container validation when
containers are unavailable; state the limitation and the exact lane a
container-capable agent or hosted CI should run next.

`devtools/local-validation-plan.sh` is still useful in these environments when
the repository checkout and basic shell tools are available: run it in default
plan mode to classify the change. Use `--run` only for classifications whose
selected checks can actually run without containers, such as agent-doc-only,
internal-doc-only, local-validation-tooling, or rendered-docs when the docs
toolchain is available. For code, testbench, workflow, or focused container
test classifications, a no-container agent should report the recommended lanes
instead of trying to replace them with weaker local evidence.

Missing optional tools such as `shellcheck`, `checkbashisms`, `actionlint`,
`zizmor`, `hadolint`, `trivy`, `jscpd`, `pycodestyle`, Cubic, Docker, or
Podman are not fatal by themselves. Record which checks could not run, run the
available applicable subset, and name the missing checks or container lanes
that still need coverage.

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
