# AI Prompt Assets for rsyslog

This directory contains maintained prompt assets that agents may read during
rsyslog development, review, documentation, and support workflows.

## Purpose

The `ai` directory keeps reusable AI-facing context outside the rsyslog core.
These files are not executable validation scripts and should not launch local
AI tooling by themselves. Active agents read the relevant prompt text and apply
it to the current diff or task.

## Current Assets

- `rsyslog_memory_auditor/`: memory ownership, cleanup, leak, and NULL-check
  review prompts for C changes.
- `rsyslog_bug_finder/`: resource, concurrency, lock-order, and lifecycle
  review prompt.
- `rsyslog_commit_assistant/`: commit message guidance.
- `rsyslog_doc_assistant/` and `rsyslog_code_doc_builder/`: documentation
  prompt assets.
- `rsyslog_issue_assistant/` and `support_gpt/`: issue and support prompt
  assets.

## Workflow

Prompt-based audits are intentionally manual from repository tooling. See
`.agent/skills/rsyslog_local_container_testing/SKILL.md` for when agents should
apply these prompts as late validation passes.
