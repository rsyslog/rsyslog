---
name: rsyslog_commit
description: Ensures compliance with rsyslog's strict commit message and branching policies.
---

# rsyslog_commit

This skill standardizes the final step of the development workflow: committing and contributing.

## Quick Start

1.  **Format First**: For C/H changes, agents MUST run
    `devtools/format-code.sh --git-changed` to rewrite formatting when needed.
2.  **Commit Message**: Follow the 62/72 rule and the mandatory "Why" structure.
3.  **Attribution**: Include the AI-Agent footer.

## Detailed Instructions

### 1. Pre-Commit Checklist
- **Code Style**: Agents MUST run `devtools/format-code.sh --git-changed` if
  any `.c` or `.h` files were modified and formatting may need to be applied.
  This is mandatory for C source changes before commit. For a read-only local
  validation gate, agents MUST run
  `devtools/format-code.sh --git-changed --check --check-if-available`; this
  skips with a warning if the exact configured `clang-format` executable is not
  installed. CI will not pass with improperly formatted C/H code; a missing
  local formatter is only a local tooling limitation, not permission to skip
  formatting.
- **Python Style**: If Python files changed and `pycodestyle` is installed, run `devtools/format-python.sh <changed-python-files>`. Use `devtools/format-python.sh --fix <changed-python-files>` only when you intentionally want `autopep8` rewrites. If the tools are missing, suggest installing them (`sudo apt-get install -y pycodestyle python3-autopep8` on Debian/Ubuntu) but do not block unrelated build or test validation. The shared 120-column style configuration lives in `setup.cfg`; review `autopep8` output carefully for legacy Python-2-style scripts.
- **Local Preflight Linters**: Before heavier validation or opening/updating a
  PR, run useful diff-scoped linters when installed: `shellcheck` for changed
  `*.sh`, `checkbashisms -p` for changed scripts that claim POSIX `sh`
  portability, `devtools/format-python.sh --check-if-available` for changed
  Python, `actionlint` and pinned `zizmor` for changed workflows, `hadolint`
  for changed Dockerfiles, `trivy config` for changed infrastructure/config
  files, and `jscpd` for larger changed source/test sets. Guard optional
  commands with `command -v`; if a tool is missing, suggest installing it but
  do not block unrelated build or test validation. On Debian/Ubuntu,
  `checkbashisms` is provided by `devscripts`. Do not run `cppcheck` routinely
  unless requested; it is too noisy for the rsyslog tree.
- **Validation**: Ensure the relevant build/test path passed. For PR-ready
  implementation work, prefer the local container-testing skill's change-gated
  validation over unconditional full-suite local runs.
  - **Late Prompt Audits**: For non-trivial C/H, concurrency, or test/build
    plumbing changes, follow the local container-testing skill's late
    prompt-audit stage. Read and apply the project's canned prompts directly;
    do not launch another AI CLI from repository scripts.
  - **Local Cubic**: Run local Cubic review for code changes when `cubic` is
    installed and reachable. Skip Cubic for documentation-only changes. For
    tests, workflows, build tooling, and mixed changes, use Cubic when the
    change is non-trivial, behavior-affecting, security-sensitive, or large.
    Hosted Cubic/Gemini PR comments are additional review feedback, not a
    substitute for local Cubic where local Cubic applies.
  - **Container Gate**: For implementation changes, run the
    `rsyslog_local_container_testing` skill's PR-ready change-gated local
    container sequence when container tooling is available. Focused
    `run-ci.sh TEST=...` commands are targeted container tests, not PR-ready
    local container validation unless that skill explicitly permits the reduced
    lane.
  - **Mock Smoke Check**: If you added or renamed test files, run `make distcheck TEST_RUN_TYPE=MOCK-OK -j$(nproc)` as a final distribution check.
  - **Note**: If you already successfully built and tested your changes immediately *before* formatting, you do NOT need to re-run the build/test cycle. Formatting is a normalization step and does not affect functionality. A read-only formatting check is already part of the deterministic local validation helper for changed C/H files.

### 2. Commit Message Structure
Rsyslog requires rich, structured commit messages (plain ASCII).
- **Title**: `<component>: <action>` (Max 62 characters).
- **Body**: Max 72 characters per line.
- **GitHub Issues**: Use **full URLs** (e.g., `https://github.com/rsyslog/rsyslog/issues/883`) instead of shorthand `#883`.
- **Mandatory Sections**:
  - **Why**: Brief non-technical rationale.
  - **Impact**: One line if behavior/tests changed.
  - **Before/After**: One-line summary.
  - **Technical Overview**: 4–12 lines describing the change conceptually.
- **AI Footer**: `With the help of AI-Agents: <agent-name>`

### 3. Using the Assistant
- **Offline/Agents**: Use the base prompt at `ai/rsyslog_commit_assistant/base_prompt.txt`.
- **Web**: [rsyslog.com/tool_rsyslog-commit-assistant](https://www.rsyslog.com/tool_rsyslog-commit-assistant)

### 4. Branching & PRs
- **Base Branch**: Always target `main`.
- **Naming**: `i-<issue-number>` or `<agent-name>-i-<issue-number>`.
- **Target**: PRs must target `rsyslog/rsyslog` directly.

## Related Skills
- `rsyslog_build`: To verify the code before committing.
- `rsyslog_test`: To provide validation metrics for the commit message.
