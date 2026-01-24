---
name: rsyslog_commit
description: Ensures compliance with rsyslog's strict commit message and branching policies.
---

# rsyslog_commit

This skill standardizes the final step of the development workflow: committing and contributing.

## Quick Start

1.  **Format First**: Run `devtools/format-code.sh`.
2.  **Commit Message**: Follow the 62/72 rule and the mandatory "Why" structure.
3.  **Attribution**: Include the AI-Agent footer.

## Detailed Instructions

### 1. Pre-Commit Checklist
- **Code Style**: Run `bash devtools/format-code.sh` IF any `.c` or `.h` files were modified. This is mandatory for C source changes.
- **Validation**: Ensure `make -j$(nproc) check TESTS=""` passes and relevant tests are run.
  - **Multi-Pass AI Audit**: Run the `/audit` workflow for a rigorous, persona-based review (Memory, Concurrency, Standards) using the project's canned prompts.
  - **Mock Smoke Check**: If you added or renamed test files, run `make distcheck TEST_RUN_TYPE=MOCK-OK -j$(nproc)` as a final distribution check.
  - **Note**: If you already successfully built and tested your changes immediately *before* formatting, you do NOT need to re-run the build/test cycle. Formatting is a normalization step and does not affect functionality.

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
