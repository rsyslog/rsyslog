# Continuous Issue Session Ledger Template

Keep this ledger in a local, non-committed path such as
`.agent/local/continuous-issue-session/<session>.md` or the repository's
existing local memory/cache location.

```text
Session: <name>
Issue class: <segfault/security/module/etc.>
Active set target: <2|3|custom>
Cadence: <shared sleep interval>
Local cache source: <path>
Started: <date/time>

Open conflict areas:
- <PR>: <files/subsystems>

Active units:
- Unit: <issue or cluster>
  Issue(s): <number/title/url>
  Selection reason: <why this is next>
  Local cache evidence: <path/key/summary>
  Pre-code triage:
    Classification: <already solved/answer-only/doc-only/test-only/code-needed/unclear>
    Evidence checked: <issue/current code/docs/tests/linked PRs>
    Action: <comment/close/docs PR/code PR/report/refill>
  Worktree: <path>
  Branch: <branch>
  PR: <url or pending>
  Head SHA: <sha>
  Touched files: <files>
  Conflict check: <result>
  Validation:
    Host focused: <commands/results>
    Local Cubic: <command/result or blocker>
    Container image: <tag> <image id>
    Static analyzer: <command/result/log>
    Ubuntu 26.04 container: <command/result/log>
    Specialist lanes: <commands/results or why not needed>
    Relaxed/skipped lanes: <lane/rationale>
    Fully container-validated: <yes/no>
  AI/review status: <threads/comments/actions>
  CI status: <checks/failures/reruns>
  Disposition: <active/merged/blocked/unclear/etc.>

Completed units:
- Unit: <issue or cluster>
  PR: <url>
  Merged/closed at: <time>
  Cleanup:
    Worktree status before removal: <clean/generated-only/dirty>
    Worktree removed: <yes/no/path>
    Local branch deleted: <yes/no/branch>
    Remote branch deleted: <yes/no/already gone/skipped with reason>
    Active set updated: <yes/no>
    Local issue cache updated: <yes/no/path/evidence>
  Issue disposition: <closed/commented/left open>
  Evidence: <summary>

Session errors:
- Unit/PR: <id>
  Command: <command>
  Log path: <path>
  Failure summary: <summary>
  Attempts: <1-4 concrete attempts>
  Host validation passed: <yes/no>
  Carry-forward decision: <continue/pause/escalate>

Needs human decision:
- Issue: <url>
  Why unclear: <summary>
  Evidence gathered: <summary>
```
