---
name: rsyslog_issue_triage
description: Triage rsyslog GitHub issues one issue or cluster at a time, classify stale and current reports, draft or post closure comments, and maintain a local evidence board.
---

# rsyslog_issue_triage

This skill standardizes rsyslog GitHub issue triage. It is meant for backlog
work where issues may be old, duplicated, already fixed, answerable by
configuration, or still valid feature and bug candidates.

## Quick Start

1. Refresh `main` before each triage unit.
2. Work in a dedicated worktree and branch for the issue, cluster, or batch.
3. Keep a non-git local triage board and cache downloaded issue data.
4. Process small batches with evidence from GitHub, current code, docs, tests,
   and relevant subtree guides.
5. Close only when the evidence is strong and the required comment has been
   posted.

## Workspace Rules

- Use a Linux-compatible environment for rsyslog repository work. When working
  under Windows, use WSL.
- Always refresh `main` first:

  ```bash
  git fetch origin main:main
  ```

- Create a dedicated worktree and branch for each triage unit. A unit may be a
  single issue, a small batch, or a larger cluster:

  ```bash
  git worktree add -b codex/triage-<unit> ../rsyslog-triage-<unit> main
  ```

- Keep triage notes under a non-git local path such as:

  ```text
  .agent/local/issue-triage/work/<unit>.md
  .agent/local/issue-triage/cache/
  ```

- Cache issue JSON or rendered issue summaries when processing large clusters.
  This avoids repeated GitHub API reads and makes later passes faster.
- Preserve user or maintainer changes in the worktree. Do not revert unrelated
  changes.

## Evidence Checklist

For each issue, inspect enough context to support the verdict:

- Issue body, comments, labels, milestone, linked PRs, referenced versions, and
  recent updates.
- Whether the issue was opened by `rgerhards`; exclude those from broad
  user-issue clustering unless explicitly asked.
- Current code with targeted `git grep` or `rg`.
- Current documentation under `doc/source/` and generated website behavior when
  relevant.
- Existing tests and ChangeLog entries.
- Relevant subtree guidance:
  - `plugins/AGENTS.md`
  - `contrib/AGENTS.md`
  - `runtime/AGENTS.md`
  - `doc/AGENTS.md`
  - `tests/AGENTS.md`
  - `tools/AGENTS.md`

When using GitHub, prefer authenticated `gh` or the GitHub connector. If writes
are not explicitly authorized, produce draft comments only.

## Classification Taxonomy

Use these verdicts consistently:

- `already implemented`
- `not reproducible / needs reporter info`
- `valid non-code answer`
- `simple code/feature candidate`
- `larger design decision needed`
- `duplicate / superseded`
- `plausible bug / needs validation`
- `valid feature candidate`
- `partially implemented`

Do not close a valid feature or plausible current bug just because it is old.
Old valid issues should be marked as triaged and left open with the next action.

## Comment And Closure Policy

GitHub writes are draft-only unless the user explicitly permits comments and
closures for the current triage run.

When closing an issue, post the reasoning first, then close. The comment should:

- Explain the evidence and why the issue is being closed.
- Mention current code/docs/tests when they are part of the decision.
- Tell users how to proceed if the problem persists.
- For issues older than 2 months, apologize for the late reply.
- For issues older than 6 months, also explain that the answer may no longer be
  relevant for the original reporter, but is being added for people arriving via
  search and while working through the backlog.
- For older issues, say that rsyslog does not lightly close issues and does not
  close them just because of age.
- For current or recent issues, ask the reporter to update the issue if the
  problem persists.
- For older issues, ask users to open a new issue if the problem reappears or
  persists on a current version.

Closure candidates include:

- Already fixed in current code, with fix commit or current behavior evidence.
- Current docs now answer the question.
- Reporter confirmed the answer works or explicitly said the issue can close.
- External logs are gone and the issue lacks enough retained evidence to
  reproduce.
- Very old support reports that lack version, config, debug output, and current
  reproduction details.

Keep open when:

- There is a valid feature request or design tradeoff.
- Compatibility, release policy, config semantics, security behavior, or broad
  architecture is involved.
- A plausible current bug remains but needs targeted validation.
- Only part of the issue is implemented.

## Non-Code Answers

For configuration or support questions, prefer a concise answer that gives a
usable path:

- State what rsyslog currently does.
- Name the relevant config pattern or module.
- Provide a minimal conceptual example when helpful.
- Distinguish syslog metadata from application-level fields.
- Point to current docs when they are known.

Common examples from the first triage run:

- `imfile` `Tag`, `Severity`, and `Facility` are input metadata/defaults, not
  per-line derived values. Parse embedded app severity into variables and route
  or format on that value.
- Templates format already-parsed data; they do not re-run the syslog parser
  over `%msg%`. Use `mmnormalize`, `pmnormalize`, or custom parsing for nested
  messages.
- Multiple `omfile` actions writing the same physical file have independent
  descriptors and rotation state. Prefer one shared output action/ruleset.

## Code Change Eligibility

Only start implementation when the user has not restricted the session to
non-code triage and the change is localized and testable.

Allowed small candidates:

- Parser fixes with clear expected behavior.
- Documentation fixes.
- Testbench fixes.
- Localized module behavior.
- Narrow feature additions with explicit semantics.

Defer to maintainer/user decision for:

- Architecture changes.
- Compatibility or release-policy risk.
- Config language semantics.
- Broad performance behavior.
- Security policy changes.
- Ambiguous reporter intent.

Use rsyslog validation skills for implementation work:

- `rsyslog_build`: `make -j$(nproc) check TESTS=""`.
- `rsyslog_test`: run targeted `./tests/<test>.sh`.
- Subtree guides and module metadata for area-specific constraints.

## Output Template

For each issue, record this in the local board:

```text
- [x] #<number> <title> (<author>, created <date>, updated <date>, labels: <labels>)
  - Verdict: <classification>.
  - Evidence: <code/docs/tests/comments checked>.
  - Recommended action: <close/comment/label/defer/implement>.
  - Draft reply: <only for non-code or closure-ready cases>.
  - Fix sketch: <only for simple code or feature candidates>.
  - Risk/test notes: <targeted validation needed>.
```

For triage units, keep a short summary at the top or bottom. The unit can be a
single issue; after the backlog is mostly processed, that is expected to become
the regular case.

```text
Triage unit: <name or issue number>
Total: <n>
Closed/commented: <n>
Kept open: <n>
Needs validation: <n>
Likely implementation candidates: <n>
Useful follow-up cluster: <name>
```

## Cluster Workflow

When asked to cluster open issues:

- Exclude issues authored by `rgerhards` unless asked otherwise.
- Fetch issue metadata and cache it locally.
- Group by likely subsystem and work type, for example:
  - docs/questions/support
  - config parser/RainerScript/YAML
  - input modules
  - output modules
  - TLS/network
  - packaging/systemd/distro
  - imfile/logrotate/state
  - queues/rate-limit/performance
  - testbench/CI/flaky
  - security/auth/permissions
- Maintain a local walking board so the user can pick a cluster, batch, or
  single issue and resume it.
- Recommend the first cluster by likely close rate, evidence availability, and
  low implementation risk.

## Final Summary

When finishing a triage unit, report:

- Whether the triage unit is fully processed.
- Count of checked, closed, kept open, and remaining unchecked issues.
- URLs for issues closed in the triage unit.
- The path to the local board.
- Notable follow-up clusters or implementation candidates.

Keep the summary concise. Include limitations such as unavailable logs, skipped
tests, or deferred implementation decisions.
