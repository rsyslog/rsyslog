Internal tooling
================

The ``devtools`` directory contains small helpers used during development
and in continuous integration.  One of them is ``rsyslog_stylecheck.py``
which verifies adherence to the coding style.

Usage::

   python3 devtools/rsyslog_stylecheck.py [options] [files...]

When no file is specified the script recursively scans the current
working directory.  The ``--max-errors`` option controls how many errors
are displayed.  Use a number to limit the output or ``all`` to disable
the limit.  The default is 100.

The checker reports trailing whitespace, invalid indentation and lines
that exceed the allowed length.  It exits with a non-zero status when
violations are found.

Local CI container user mode
----------------------------

Most local ``devtools/devcontainer.sh`` runs should use the default user mapping
so generated files remain owned by the host developer.  Some CI-parity checks
intentionally override this with ``RSYSLOG_CONTAINER_UID=''`` so the container
runs with root semantics.  This is useful when a local run needs to mirror
GitHub Actions behavior closely, including service startup and sudo-backed
setup steps.

The tradeoff is cleanup risk: root-mode container runs can leave root-owned
generated files in the shared worktree.  Before switching compiler, sanitizer,
configure options, container image, or returning to host-side testing, clean
the tree and remove generated test helper binaries.  If a root-owned generated
tree blocks cleanup, fix ownership only for that generated tree before removing
it; do not use broad recursive cleanup outside the worktree.

CI failure evidence artifacts
-----------------------------

The pull request check workflow can upload short-lived artifacts for failed CI
matrix attempts.  This is controlled by the workflow environment variable
``RSYSLOG_UPLOAD_FAILURE_ARTIFACTS``.  Set it to ``1`` to enable uploads; leave
it unset or set to any other value to disable uploads for that workflow.

The upload step runs only for failed matrix attempts.  Cancelled attempts are
intentionally ignored because PR pushes often cancel in-flight runs before they
produce useful flake evidence.  Artifact names include the workflow run id, run
attempt, and matrix lane.  This is intentional: a rerun must not overwrite or
hide evidence from an earlier failed attempt.  Successful reruns therefore do
not remove the previous failed attempt artifact.

Before upload, the workflow normalizes read permissions on the expected log
paths.  Container CI can leave some files owned by the container user, and the
artifact action must be able to read every file matched by the upload globs.

These artifacts are a transport mechanism, not the permanent flake database.
Collectors should download the artifact, condense the logs into the shared
flake evidence store, and may delete the GitHub artifact after the condensed
record has been committed and pushed.  The workflow keeps a short retention
period so missed collector passes do not accumulate long-term storage.
