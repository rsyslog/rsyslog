---
name: rsyslog_changelog
description: Maintain rsyslog ChangeLog entries by selecting important user-visible changes, matching the release-note style, and avoiding low-signal commit-log duplication.
triggers:
  - ChangeLog
---

# rsyslog_changelog

Use this skill when updating the top-level `ChangeLog`, preparing release
notes, or deciding whether recent commits belong in a scheduled release block.

## Quick Start

1.  Read the current release block and at least one previous release block.
2.  Find the last substantive `ChangeLog` maintenance commit and review later
    non-merge commits plus relevant merge PR titles.
3.  Add only important user-visible, maintainer-visible, or packaging/build
    compatibility changes.
4.  Match the existing prose style and keep umbrella churn out of individual
    entries.
5.  Validate with `git diff --check`.

## Selection Rules

Add entries for changes that are useful to release-note readers:

- new modules, new operating modes, new important configuration parameters
- bug fixes with meaningful operational impact
- compatibility, portability, packaging, or build-system changes that
  maintainers need to know about
- security hardening or robustness work only when it is specific enough to
  describe clearly, or when the current release block does not already cover it
  with an umbrella item
- deprecations, warnings, defaults, or behavior changes users may notice

Usually skip:

- CI-only churn, dependency bumps, artifact collection, and workflow cleanup
- testbench de-flaking when the release block already has a testbench umbrella
- broad hardening commits when the release block already has a hardening
  umbrella and there is no distinct user-visible behavior
- typo fixes, small documentation nits, issue-triage-only changes, and agent
  workflow changes unless maintainers explicitly need them in release notes
- duplicate entries for a topic already covered by a better umbrella or earlier
  specific entry

When unsure, prefer not adding an entry unless the commit would help a user,
packager, maintainer, or support engineer decide whether the release matters.

## Discovery Workflow

- Start from a fresh `main` and work in a sibling worktree, following the repo
  `AGENTS.md` and `AGENTS.local.md` rules.
- Read enough style samples before editing:
  - `sed -n '1,180p' ChangeLog`
  - one older completed release block if the current block is sparse
- Identify the baseline:
  - `git log --format='%H%x09%cs%x09%s' -n 12 -- ChangeLog`
  - treat "maintain ChangeLog" and release-block edits as likely baselines
  - if a later `ChangeLog` commit only deduplicated or formatted entries,
    inspect before assuming it covered recent commits
- Review candidate commits since the baseline:
  - `git log --format='%H%x09%cs%x09%s' --since='<date>' --reverse`
  - include merge PR subjects when they reveal feature scope
  - use `git show --stat --format=fuller <commit>` for candidates before
    writing release prose

## Style

- Insert new entries in the current scheduled release block, newest first,
  unless the existing block has a clear different order.
- Keep entries in this form:

  ```text
  - YYYY-MM-DD: component: concise change summary
    One or more wrapped prose lines that explain behavior and impact.
    Closes https://github.com/rsyslog/rsyslog/issues/NNNN
  ```

- Match the local wording style: concise, operational, and factual.
- Wrap prose to the surrounding file style, normally about 72 columns.
- Prefer `Closes ...`, `Fixes ...`, or `See ...` lines only when the source
  commit or PR provides a reliable issue URL.
- Use `IMPORTANT FOR MAINTAINERS` only for packaging, dependency, default,
  or release-process changes maintainers must act on.
- Keep umbrella items short and explicit, for example `general hardening` or
  `testbench de-flaking`; do not also list every commit covered by them.
- Preserve existing historical typos and wording outside the edited area.

## Validation

- Run `git diff --check`.
- Re-read the edited release block to catch duplicate topics and ordering
  mistakes.
- Check for overlong prose, typos, and spelling errors.
- Remove entries that are really CI/test-only churn.
- For changelog-only edits, full build and container validation are normally
  unnecessary; say explicitly that only text validation was run.

## Related Skills

- `rsyslog_commit`: For the final commit message.
- `rsyslog_doc`: Use only when changing documentation under `doc/`.
