# AGENTS.md – Core plugins subtree

These instructions apply to everything under `plugins/` (except `plugins/external`,
which vendors third-party code; do not modify it unless specifically requested).

## Build & bootstrap reminders
- Run `./autogen.sh` before your **first** build in a fresh checkout and
  whenever you touch `configure.ac`, any `Makefile.am`, or files under `m4/`.
  The bootstrap step can take up to ~2 minutes, so skip it for pure
  documentation or metadata-only edits when no build is required.
- Configure with the options needed for the module you are touching.  The
  defaults build all modules whose dependencies are present.  Use
  `./configure --help` to discover `--enable`/`--disable` switches if you need
  to constrain the build.
- Keep the testbench enabled with `./configure --enable-testbench` so
  module-specific tests continue to compile.
- Build with `make -j$(nproc)` and execute the most relevant smoke/regression
  test directly (for example `./tests/imtcp-basic.sh` or a module-specific
  script).  Direct invocation keeps stdout/stderr visible.  Use `make check`
  only when mirroring CI or chasing harness-specific failures.

## Module-level agent guides
High-complexity modules benefit from their own `AGENTS.md` living directly
inside the module directory (for example `plugins/omelasticsearch/AGENTS.md`).
Use them to capture:

- A short overview plus links to user-facing docs.
- Build or dependency switches that only affect this module.
- Smoke/regression tests to run locally and any helper scripts to prepare
  external services.
- Common troubleshooting tips (log messages, stats counters, error files).
- Coordination notes when touching shared helpers or cross-module contracts.
- References to `MODULE_METADATA.yaml` and other metadata that must stay in
  sync.

Copy `plugins/MODULE_AGENTS_TEMPLATE.md` when creating a new module guide and
update it whenever workflows or dependencies change.

## Metadata required for every module
Each module directory (for example `plugins/omfile/`) should contain a
`MODULE_METADATA.yaml` file.  The file communicates ownership and expectations
for both humans and agents.

### Required keys
```yaml
support_status: core-supported | contributor-supported | stalled
maturity_level: fully-mature | mature | fresh | experimental | deprecated
primary_contact: "GitHub Discussions & Issues <https://github.com/rsyslog/rsyslog/discussions>"
last_reviewed: YYYY-MM-DD
```

#### Support status values
- `core-supported`: Maintained by the rsyslog core team.
- `contributor-supported`: Maintained primarily by a community contributor; the
  core team reviews but does not lead feature work.
- `stalled`: No active maintainer; use extra caution before changing behavior.

#### Maturity level values
- `fully-mature`: Long-standing module with well-established interfaces.
- `mature`: Stable but with medium deployment footprint or recent major changes.
- `fresh`: Recently added or lightly used; expect feedback-driven revisions.
- `experimental`: Works, but explicitly needs broader testing feedback before it
  can be considered stable.
- `deprecated`: Kept only for backward compatibility; avoid new feature work and
  document migration paths in user-facing docs.

### Optional keys
Add keys as needed to help future contributors:
- `build_dependencies`: List library or tool requirements (match configure
  options when possible).
- `runtime_dependencies`: Libraries or services the module needs at runtime.
- `ci_targets`: Names of CI jobs or scripts that exercise this module.
- `documentation`: Links into `doc/` or external references.
- `support_channels`: Overrides or supplements the default GitHub Discussions
  and Issues flow when a module has a bespoke support process.
- `notes`: Free-form guidance for reviewers and contributors.

Replace `primary_contact` with a specific maintainer string (e.g.
`"Full Name <email@example.com>"`) when a module has an active owner outside the
standard GitHub Discussions and Issues queue.

### Template
Copy `plugins/MODULE_METADATA_TEMPLATE.yaml` when creating or updating
module metadata.  Keep values accurate and review them whenever ownership or
support expectations change.

## Coding expectations
- Follow `MODULE_AUTHOR_CHECKLIST.md` for concurrency and documentation tasks.
- Update the module's top-of-file "Concurrency & Locking" comment when
  behavior changes.
- Update `doc/ai/module_map.yaml` if concurrency expectations change.
- When adding a new module, update `plugins/Makefile.am`, `configure.ac`, and
  provide tests under `tests/` to cover the new behavior.
- **Documentation**: Every new configuration parameter **must** be documented in
  `doc/source/reference/parameters/` and linked from the module's `.rst` file.

## Testing expectations
- Prefer module-focused tests in `tests/` named after the module (e.g.
  `omxyz-basic.sh`).
- Ensure new tests run cleanly when invoked directly via `./tests/<script>.sh`.
  When reviewers request a CI reproduction, confirm the test also passes under
  `make check`.
- When a module requires external services (databases, message queues, etc.),
  document setup steps in the module metadata and reference any helper scripts
  placed under `tests/`.

## Documentation touchpoints
- Mention significant behavior or dependency changes in `doc/` (for example,
  module reference guides or changelog entries).
- Cross-link any new documentation from the appropriate `index.rst` so users
  can discover it easily.
