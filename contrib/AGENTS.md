# AGENTS.md – Contrib modules subtree

These instructions apply to everything under `contrib/`.

## Expectations for contrib work
- Contrib modules are not part of the core support contract.  Changes should
  preserve backward compatibility for existing users and clearly call out
  behavior shifts in commit messages and documentation.
- Many contrib modules depend on third-party SDKs or services that are not
  available in CI.  Document any manual setup that reviewers must perform.

## Build & bootstrap reminders
- **Efficient Build:** Use `make -j$(nproc) check TESTS=""` to incrementally build the core and all test dependencies. This is the primary build command.
- **Bootstrap/Configure:** Only run `./autogen.sh` and `./configure` if:
    1.  The `Makefile` is missing (first run).
    2.  You have modified `configure.ac`, `Makefile.am`, or `m4/` files.
    3.  You need to change build options (e.g., enabling a new contrib module).
    4.  You need to enable a module that requires specific flags (check `MODULE_METADATA.yaml`).
- **Run Tests:** Execute the most relevant smoke/regression test directly (e.g., `./tests/imtcp-basic.sh`). Direct invocation keeps stdout/stderr visible. Use `make check` only when mirroring CI.

## Metadata required for every module
Each contrib module directory (for example `contrib/mmkubernetes/`) must contain
`MODULE_METADATA.yaml`.  Contrib metadata uses the same schema as core plugins,
with different default expectations.

### Required keys
```yaml
support_status: contributor-supported | stalled
maturity_level: fully-mature | mature | fresh | experimental | deprecated
primary_contact: "GitHub Discussions & Issues <https://github.com/rsyslog/rsyslog/discussions>"
last_reviewed: YYYY-MM-DD
```

- Default `support_status` is `contributor-supported` unless the core team has
  formally adopted the module.
- Use `stalled` if no maintainer is known.  Do not set `core-supported` unless
  the module has moved to `plugins/`.

### Optional keys
Use the optional keys from the template to document build/runtime
requirements, CI coverage, and reviewer notes.  Copy
`contrib/MODULE_METADATA_TEMPLATE.yaml` when creating the file.

- `build_dependencies`: List library or tool requirements (match configure
  options when possible).
- `runtime_dependencies`: Libraries or services the module needs at runtime.
- `ci_targets`: Names of CI jobs or scripts that exercise this module.
- `documentation`: Links into `doc/` or external references.
- `support_channels`: Overrides or supplements the default GitHub Discussions
  and Issues flow when a module has a bespoke support process.
- `notes`: Free-form guidance for reviewers and contributors.

Replace `primary_contact` with a specific maintainer string when a contrib
module has an active owner outside the standard GitHub Discussions and Issues
queue, and record any bespoke escalation path via the optional
`support_channels` array if needed.

## Testing expectations
- Prefer smoke tests that can run directly via `./tests/<script>.sh` without
  proprietary services.  If that is impossible, provide script stubs in `tests/`
  that mock or skip the integration but keep API coverage verifiable, and note
  any CI limitations in the metadata.
- Record any external test environments (container images, cloud resources) in
  the module metadata so reviewers understand the manual steps required.

## Documentation touchpoints
- Update `doc/` to mention new or changed contrib modules, especially when the
  module has prerequisites or manual installation steps.
- When a contrib module becomes core-supported, move it under `plugins/`, update
  the metadata accordingly, and inform maintainers via the changelog.
