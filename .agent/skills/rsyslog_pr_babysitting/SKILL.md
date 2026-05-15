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
- Stop babysitting when all monitored PRs are green: no failed checks, no
  running or queued required checks, and no unresolved actionable review
  threads. Do not keep polling until merge unless explicitly asked.
- Do not leave background sleep or polling processes alive when stopping work.
- Report the commit SHA, failed checks, running checks, and unresolved review
  findings at each meaningful update.

## CI Poll

Use `gh pr view` for check rollups:

```sh
gh pr view PR_NUMBER --repo rsyslog/rsyslog \
  --json statusCheckRollup,reviewDecision,mergeable,state,headRefOid,url
```

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

Do not reply on GitHub, resolve threads, or push fixes unless explicitly asked.

## Status Summary

When reporting, separate:

- CI failures that need code changes
- likely flakes already rerun
- unresolved review comments needing decisions
- checks still running

If all checks pass but unresolved actionable review threads remain, say the PR is
not fully babysat yet.
