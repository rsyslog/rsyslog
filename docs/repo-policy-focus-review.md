# Repo Policy Focus Review

This workflow adds a small repository-policy review for pull
requests. It does not try to rate the whole patch. Instead, it focuses on three
high-value policy areas that are not covered well by the existing CI suite:

- test registration under `tests/`
- documentation distribution sync under `doc/source/`
- new-module onboarding under `plugins/` and `contrib/`

## Why this exists

The repository already has strong generic CI for formatting, linting, CodeQL,
docs, and the build/test matrix. What it lacked was rsyslog-specific policy
feedback in areas where contributors often need repository knowledge instead of
generic code review.

This workflow uses deterministic fact collection and deterministic evaluation to
turn those fact bundles into `pass`, `warn`, `fail`, or `not_applicable`
results.

## Focused checks

### `tests-registration`

Applied when a pull request adds or renames shell tests in `tests/`.

The package builder collects facts such as:

- new test script paths
- whether `tests/Makefile.am` mentions those scripts
- whether new `-vg.sh` wrappers appear to source the base scenario

### `doc-dist-sync`

Applied when a pull request changes `.rst` files under `doc/source/`.

The package builder collects facts such as:

- changed doc paths
- missing `doc/Makefile.am` `EXTRA_DIST` entries for added or renamed docs
- stale `EXTRA_DIST` entries left behind for deleted or renamed docs

### `module-onboarding`

Applied when a pull request appears to add a new module under `plugins/` or
`contrib/`.

The package builder collects facts such as:

- new module directories relative to the base revision
- whether `MODULE_METADATA.yaml` exists
- whether a likely module doc touchpoint exists under `doc/source/`

## Workflow behavior

- always builds the focused review package
- only runs the evaluator when at least one focused check is applicable
- writes a workflow summary
- fails the workflow when an applicable check returns `fail`

The workflow follows the normal CI failure path so policy failures are visible
in the standard checks UI and logs.

## Files

- workflow: `.github/workflows/repo-policy-focus-review.yml`
- builder: `scripts/build_repo_policy_focus_input.py`
- evaluator: `scripts/evaluate_repo_policy_focus.py`
- scorer: `scripts/score_repo_policy_focus.py`
