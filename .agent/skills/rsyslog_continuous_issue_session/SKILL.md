---
name: rsyslog_continuous_issue_session
description: Orchestrate long-running rsyslog issue-fix sessions with a rolling active set of work units, local issue cache use, separate worktrees, full validation gates, PR babysitting, merged-PR cleanup, and automatic refill until the selected issue class is exhausted.
---

# rsyslog_continuous_issue_session

Use this skill for long-running rsyslog backlog sessions where the user wants an
agent to keep processing a selected class of issues, such as segfault-related
reports, while maintaining a small active set of open PRs.

This is an orchestration skill. It does not replace the build, test, container,
triage, commit, or PR babysitting skills. Use those skills at the points named
below and follow their stricter local instructions.

## Core Contract

Maintain a rolling active set of issue/PR work units until the selected issue
set is exhausted or the user pauses the session.

- Default active set size is 2 to 3 work units unless the user sets another
  number.
- A work unit may be one issue or a tightly related issue cluster that should
  share one PR.
- Each work unit gets its own sibling worktree, branch, validation ledger entry,
  commits, and PR.
- On every babysit pass, if **any** active PR has merged, closed, become
  obsolete, or otherwise completed, immediately clean it up and refill that
  active slot with the next eligible non-conflicting issue.
- Do not wait for all active PRs to finish before starting the next work unit.
- Keep polling all active PRs on one shared cadence. Do not start per-PR timers.
- Do not use automations unless the user explicitly asks; when the user asks for
  sleeps, use a single shared sleep and do no work while sleeping.

## Startup Checklist

1. Read repository `AGENTS.md`, `AGENTS.local.md` if present, and relevant
   subtree guides.
2. Load this skill, then use:
   - `rsyslog_issue_triage` for issue classification, local cache use, closure
     comments, and evidence boards.
   - `rsyslog_build`, `rsyslog_test`, and `rsyslog_local_container_testing` for
     validation.
   - `rsyslog_commit` for formatting, commit messages, branch, and PR hygiene.
   - `rsyslog_pr_babysitting` for check polling, review threads, AI comments,
     reruns, and CI failure classification.
3. Recover the local issue cache and session ledger before querying GitHub.
   Prefer local memory for known issue lists, dispositions, and prior evidence.
   Refresh GitHub only for state that may have changed: issue open/closed state,
   linked PRs, comments, PR checks, and review threads.
4. Refresh upstream `main` and sync the fork main according to local
   instructions before creating or updating task worktrees.
5. Inspect currently open PRs and avoid overlapping touched areas unless the
   selected issue cannot be handled otherwise.
6. Choose enough eligible work units to fill the active set.

## Work Unit Selection

For each candidate issue:

- Check local cache first, then refresh the issue's current GitHub state.
- Analyze the issue before assuming a code change is needed. Decide whether the
  correct action is code, documentation, a test-only change, a configuration or
  support answer, already-solved closure, duplicate/superseded closure, or
  human decision.
- Exclude issues already fixed, already closed, duplicated, superseded, or made
  obsolete by merged code. Post the required evidence comment and close only
  when the conclusion is clear and the user has authorized issue writes.
- For answer-only or documentation-only issues, do not force a code PR. Post or
  draft the answer, or make the narrow documentation update, according to the
  user's write permissions for the session.
- Keep unclear or under-specified issues open. Record the evidence and report
  them as needing human decision.
- Prefer non-conflicting modules or code areas when active PRs already touch
  nearby files.
- Group issues only when they share the same code area, failure mode, and test
  surface. Otherwise keep one issue per PR.
- If the selected issue class is crash or segfault related, prioritize issues
  with current code paths, reproducible inputs, or clear unsafe lifecycle,
  pointer, bounds, or error-path evidence.

## Per-Unit Implementation Loop

For each active slot, process one work unit end to end:

1. Create or reuse a dedicated sibling worktree and `codex/` branch.
2. Run the pre-code triage gate below. Continue to code only when the gate says
   a code, test, or documentation change is still needed.
3. Analyze the issue, current code, linked PRs, tests, and module metadata.
4. Reproduce or build the strongest local evidence available before editing.
   - Journal/imjournal runtime bugs are static-analysis-only on hosts without a
     usable journal service. Do not try to force journal runtime tests there.
5. Implement the narrowest compatible fix.
6. Add or update focused regression tests when practical.
7. Validate locally using the required validation section below.
8. Format and commit using `rsyslog_commit`.
9. Push, open a PR, assign/milestone/category per local instructions, and link
   the issue with full URLs where applicable.
10. Do one immediate babysit pass after PR creation or push, then return to the
   shared active-set cadence.

## Pre-Code Triage Gate

Before editing files, classify the work unit using `rsyslog_issue_triage`:

- `already solved`: verify current behavior, comment with evidence, close when
  authorized, update local cache, and refill the slot.
- `duplicate / superseded`: record the canonical issue or PR, comment/close
  when authorized, update local cache, and refill the slot.
- `answer-only`: post or draft the support/configuration answer, leave or close
  according to the triage rules, update local cache, and refill the slot.
- `documentation-only`: make only the narrow documentation update and validate
  documentation-local requirements; do not invent runtime code changes.
- `test-only`: fix or add the missing regression/testbench coverage and run the
  relevant test validation.
- `code-needed`: proceed with implementation and full validation.
- `unclear`: do not close and do not code speculatively. Record the blocker and
  report the issue for human decision, then refill the slot.

## Required Validation

Use the existing validation skills. Do not downgrade their requirements here.

- Start with focused host-side validation from `rsyslog_test` while iterating.
- Use direct `tests/*.sh` execution for focused shell tests.
- Use `diag.sh` helpers such as `check_content`, `cmp_exact`, and
  `require_plugin`; prefer asserting rsyslog diagnostics through configured
  output files after synchronized shutdown unless a test documents an exception.
- For new or renamed tests, follow the `tests/Makefile.am` distribution pattern
  from `rsyslog_test` and run the mock distcheck gate when required.
- Run local Cubic review when available.
- Run `rsyslog_local_container_testing` before reporting PR-ready validation:
  static analyzer first, then the Ubuntu 26.04 container `run-ci.sh` check run,
  plus any touched-area specialist lanes required by that skill.
- Focused container tests supplement the full local container gate; they do not
  replace it unless the container-testing skill explicitly permits the reduced
  lane for the touched area.
- Use Docker Hub CI-equivalent dev images when appropriate. Use locally built
  images only when validating a locally built image or runtime container.
- For segfault, memory, parser, string, JSON, property, or bounds work, strongly
  consider the sanitizer lane required by `rsyslog_local_container_testing`.
- For lifecycle, action shutdown, queue, lock, or shared-state work, consider
  TSAN. Do not combine TSAN with Valgrind; they are incompatible.
- Journal/imjournal runtime reproduction remains static-analysis-only on hosts
  without a journal service; record that exception in the ledger.
- If a full container lane fails, inspect logs and make up to four concrete
  remediation attempts only when locally fixable. Record the failure, command,
  log path, attempts, affected PR/unit, and whether host-side validation passed,
  then continue to another work unit instead of spinning.

## Babysitting And Refill Loop

Run this loop for all active PRs together on the user-specified cadence:

1. Check PR state, head SHA, merge state, checks, review threads, flat comments,
   and AI comments.
2. If checks fail, use `rsyslog_pr_babysitting` to inspect logs before
   classifying failures. Fix PR-caused failures, rerun likely flakes, and record
   external blockers.
3. Treat AI comments as first-class review. Either implement and reply, or
   reply with the reason the comment is invalid or obsolete.
4. If any PR is merged or closed:
   - run the mandatory local cleanup phase below,
   - immediately refill the active slot with the next eligible issue.
5. If all active PRs are green but unmerged, continue only if the user requested
   merge-time monitoring or continuous refill. Otherwise follow
   `rsyslog_pr_babysitting` stop rules.

## Mandatory Local Cleanup

Run this cleanup whenever an active PR is merged, closed as obsolete, or
otherwise leaves the active set. Local cleanup is part of the continuous-session
contract because stale worktrees and branches make long-running sessions
misread local state.

1. Confirm the PR state and merged/closed commit from GitHub.
2. Check the worktree status. Remove the worktree only when it is clean or when
   all remaining files are known generated artifacts from that unit.
3. Delete the local feature branch after the worktree is removed.
4. Delete the remote feature branch when it belongs to this agent/session and no
   other open PR uses it. If GitHub already deleted it, record that as clean.
5. Update the session ledger: PR disposition, final SHA, cleanup commands,
   issue disposition, and any remote-branch deletion result.
6. Remove the work unit from the active set.
7. Remove stale or completed issues from the local issue cache only after the
   evidence is recorded. Do not remove unclear issues; keep them in the report
   for human decision.
8. Refresh the conflict-area map before selecting the replacement work unit.

## Ledger

Maintain a local ledger outside committed source files unless the user asks for
a committed report. Use the template in
`.agent/skills/rsyslog_continuous_issue_session/references/ledger-template.md`.

Each active or completed unit must record:

- issue number, title, URL, selection reason, and local cache source
- branch, worktree, PR URL, PR number, and final commit SHA
- touched files and conflict-area check
- validation commands, results, image tag and image ID, local Cubic result,
  hosted AI/review status, and any lane relaxations with rationale
- AI comments and how they were addressed
- closure/comment action for stale or obsolete issues
- final disposition: active, merged, closed-obsolete, blocked, unclear, or
  needs human decision

## Reporting

Interim updates should be short and focused on meaningful state changes:
selected issue, PR opened, validation failure, AI comment handled, PR merged,
slot refilled, or blocker recorded.

Final or pause reports must include:

- active PRs and their check/review state
- merged/closed PRs cleaned up
- new issues started because a slot opened
- issues closed as obsolete with evidence
- unclear issues left open for human decision
- validation status, explicitly distinguishing fully container-validated work
  from targeted container-tested-only work
