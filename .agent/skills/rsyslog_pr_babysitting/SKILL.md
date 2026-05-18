---
name: rsyslog_pr_babysitting
description: Monitor rsyslog pull requests after push or rerun, including GitHub Actions checks, unresolved review threads, bot comments, reruns for known flakes, and concise status reporting.
---

# rsyslog_pr_babysitting

Use this skill when babysitting an rsyslog PR after pushing, rerunning CI, or
waiting for review. Babysitting is incomplete unless both CI status and
unresolved review threads have been checked. Read review comments as part of
babysitting, including bot or CI comments that explain failed or skipped jobs.

When a user says "babysit PR", the default goal is to watch and act until the
PR is fully babysat (see stop conditions below), then stop. Do not keep
polling until merge unless the user explicitly asks for merge-time monitoring.

## Poll Cadence

- Poll every 10 to 20 minutes while checks are running; 15 minutes is the
  default.
- If checks are complete but actionable review threads or CI/bot comments
  remain unresolved, continue on the same cadence until they are handled or the
  user says to stop.
- If babysitting a non-empty set of PRs, check them together on the same
  cadence instead of starting separate tight loops.
- Stop babysitting each PR as soon as that PR is green: no failed checks, no
  running or queued required checks, and no unresolved actionable review
  threads. Merge is a maintainer decision and may happen days later, so a green
  unmerged PR is not a reason to keep polling unless the user explicitly asked
  for merge-time monitoring.
- Do not leave background sleep or polling processes alive when stopping work.
- Report the commit SHA, failed checks, running checks, and unresolved review
  findings at each meaningful update.
- Do not keep polling when progress is blocked by a maintainer decision,
  missing credentials, unavailable external service, or any other condition the
  agent cannot resolve safely. Notify once with the blocker and recommended
  next step, then stop or pause the automation.

## Poll Decision Table

Run this decision table on every poll before deciding whether to report, fix,
or keep waiting:

- **Merged**: stop tracking the PR. Worktree and branch cleanup is a separate
  maintenance task; do it only when explicitly requested.
- **Checks running or queued**: keep polling on cadence, unless review threads
  already contain an actionable simple fix that can be handled while checks
  run.
- **Checks failed**: inspect failing logs before guessing. Classify the failure
  as likely flake, PR-caused, external/unresolvable, or decision-needed.
- **Likely flake**: record the failing test and reason, rerun failed jobs only,
  and keep polling.
- **PR-caused failure**: fix only after tying the failing path to the PR's
  changes, validate locally, amend or commit as appropriate, push, and keep
  polling.
- **Active review threads**: classify each unresolved, non-outdated thread as
  `actionable/simple`, `needs maintainer decision`, `external/unresolvable`, or
  `response-only`.
- **Actionable/simple review thread**: if the PR branch is in the agent's write
  scope, fix it immediately, validate the affected area, amend or commit as
  appropriate, push, and continue babysitting. Do not report it as a blocker
  before attempting the fix.
- **Needs maintainer decision or external/unresolvable**: notify the user once
  with the exact blocker and recommended next step, then stop or pause
  babysitting for that PR.
- **Response-only**: draft the needed response for the user and stop or pause
  unless the user explicitly asked the agent to post replies.
- **Fully babysat**: stop tracking this PR when there are no failed checks, no
  running or queued required checks, and no unresolved actionable review
  threads. Do not keep polling a green PR while it waits for maintainer merge
  unless the user explicitly asked for merge-time monitoring.

## CI Poll

Use `gh pr view` for check rollups:

```sh
gh pr view PR_NUMBER --repo rsyslog/rsyslog \
  --json statusCheckRollup,reviewDecision,mergeStateStatus,mergeable,state,headRefOid,url
```

Treat `mergeable: CONFLICTING` or `mergeStateStatus: DIRTY` as an
actionable blocker, not just a status to report. Before resolving it, fetch
official upstream `main` freshly, for example:

```sh
git fetch upstream main --prune
```

Then rebase or merge the PR branch onto that fetched `upstream/main`, resolve
conflicts locally, validate the affected files, and push the updated branch.
Do not rely on the fork's `origin/main` for conflict resolution.

For failures, inspect logs before guessing:

```sh
gh run view RUN_ID --repo rsyslog/rsyslog --job JOB_ID --log-failed
gh api -H 'Accept: application/vnd.github+json' \
  /repos/rsyslog/rsyslog/actions/jobs/JOB_ID/logs
```

If a failed test is a known or likely flake, record the failing test name and
reason, then rerun failed jobs only:

```sh
gh run rerun RUN_ID --repo rsyslog/rsyslog --failed
```

Do not change code for CI failures until the failing path has been tied to the
PR's changes.

## Review Thread Poll

Fetch review threads with GraphQL. Flat PR comments are not enough because they
omit thread state. If a GitHub review-comment skill is available, use its
thread-aware fetch helper; otherwise query `reviewThreads` directly:

```sh
gh api graphql \
  -F owner=rsyslog \
  -F repo=rsyslog \
  -F number=PR_NUMBER \
  -f query='
query($owner:String!,$repo:String!,$number:Int!) {
  repository(owner:$owner,name:$repo) {
    pullRequest(number:$number) {
      reviewThreads(first:100) {
        nodes {
          isResolved
          isOutdated
          path
          line
          originalLine
          comments(first:20) {
            nodes { author { login } createdAt body }
          }
        }
      }
    }
  }
}'
```

For each unresolved, non-outdated thread, track:

- author, path, and line
- priority words such as high, critical, regression, skip, CI, or workflow
- whether the requested change is actionable or only needs a response

When the PR branch belongs to the current agent task or the user has asked the
agent to babysit its own PR, simple review comments are part of the babysitting
work. Handle small, localized fixes directly, validate them, and push the
updated branch. Examples include typo fixes, documentation wording, metadata
formatting, simple example corrections, and narrow test expectation updates.

AI review comments need an explicit GitHub reply so later readers know the
comment was considered. If the requested change was implemented, a short
`Done.` reply is sufficient. If the comment is invalid, not applicable, or not
implemented, reply with the specific reason.

Do not push fixes for PRs outside the agent's write scope, broad design changes,
compatibility changes, ambiguous review requests, or anything requiring
maintainer policy judgment. Report those once and stop or pause polling. Do not
reply to non-AI reviewers or resolve threads unless explicitly asked.

## Status Summary

When reporting, separate:

- CI failures that need code changes
- likely flakes already rerun
- unresolved review comments needing decisions
- checks still running

If all checks pass but unresolved actionable review threads remain, say the PR is
not fully babysat yet.
