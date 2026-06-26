---
name: rsyslog_v8stable_patch_flow
description: Handle rsyslog post-.0 v8-stable updates, including patch-release ChangeLog sections, PRs targeting v8-stable, preserving stable merge commit hashes, and merging v8-stable back into main cleanly.
---

# rsyslog_v8stable_patch_flow

Use this skill when a change belongs to `v8-stable` after an `.0` release has
already shipped, especially for follow-up fixes such as `8.2604.1`,
`8.2604.2`, and later patch releases. Pair it with `rsyslog_changelog`,
`rsyslog_commit`, and `rsyslog_pr_babysitting` as needed.

## Core Rule

Do not rewrite a released `.0` ChangeLog entry to describe later patch-release
behavior. Keep the `.0` entry historically accurate, and add the real
follow-up delta to the appropriate `.1`, `.2`, etc. release section.

If a patch fix reverses, narrows, or corrects behavior that was documented for
`.0`, keep both stories:

- the `.0` entry describes what the `.0` release did;
- the patch entry describes what changed after `.0`;
- issue links stay with the release entry that actually resolved that issue.

## Stable Worktree

1.  Fetch current upstream branches:

    ```sh
    git fetch upstream main v8-stable --prune
    ```

2.  Create or reuse a sibling worktree from `upstream/v8-stable`, not from
    `main`, for the stable follow-up:

    ```sh
    git worktree add -b codex/<topic> ../rsyslog-<topic> upstream/v8-stable
    ```

3.  Target the PR at `rsyslog/rsyslog:v8-stable`. Do not open these patch
    follow-up PRs against `main` unless the user explicitly asks for a main-only
    change.

## ChangeLog Patch Sections

For post-`.0` stable work, add a patch-release header near the top of
`ChangeLog` if it does not already exist:

```text
--------------------------------------------------------------------------------------
Scheduled Release 8.yymm.N (aka YYYY.MM) YYYY-MM-DD
```

Use `N=1` for the first post-`.0` patch, then `N=2`, and so on. Keep the
`aka YYYY.MM` value tied to the original `.0` monthly release. Insert entries
for the patch section in the normal rsyslog format:

```text
- YYYY-MM-DD: component: concise change summary
  Operationally useful explanation of the behavior and impact.
  Closes https://github.com/rsyslog/rsyslog/issues/NNNN
```

When moving a follow-up note out of a later or wrong release block, restore the
old block to the wording that matched its original release, then add the
follow-up entry under the patch header.

## PR And Validation

- Keep PRs draft by default unless the user asks otherwise.
- Use the usual rsyslog metadata: assign `rgerhards`, set the active release
  milestone, and choose the best-fit category label.
- For ChangeLog-only fixes, `git diff --check` plus rereading the edited
  release blocks is normally sufficient. State that no build or container
  validation was run because the change is text-only.
- For code, tests, build logic, or workflow changes, follow the normal
  rsyslog build/test/container validation skills.

## After The v8-stable PR Merges

After the stable PR is merged, make `main` contain the exact stable history
instead of cherry-picking or recreating it:

1.  Fetch both branches fresh.
2.  Fast-forward local `main` to `upstream/main`.
3.  Merge `upstream/v8-stable` into `main` with a normal merge commit, using
    `--no-ff` so the stable merge commit remains reachable even if a
    fast-forward would otherwise be possible.
4.  Resolve only expected conflicts. If the user says current `main` wins for
    a file, use `git checkout --ours <file>` and stage that file.
5.  Verify:

    ```sh
    git merge-base --is-ancestor upstream/v8-stable main
    git status --short --branch
    ```

6.  Push `main` to `upstream` if direct push is permitted.

    If `upstream/main` is protected, push the merged branch to the fork and
    open a PR targeting `rsyslog/rsyslog:main`.

    Finally, sync the fork's `main` if local workflow instructions require it.

This keeps the merge commit from the stable PR reachable from `main`, so future
`v8-stable` to `main` merges stay clean and retain the stable-side commit IDs.
