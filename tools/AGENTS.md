# AGENTS.md – Built-in tool modules

The `tools/` directory holds historic built-in modules that share code with the
core but are compiled directly into the rsyslog binary:

- `omfile.c`
- `omusrmsg.c`
- `omfwd.c`
- `ompipe.c`
- `omshell.c`
- `omdiscard.c`

## Build & testing expectations
- These modules are always built as part of the default configuration; no
  additional `./configure` flags are required.
- **Efficient Build:** Use `make -j$(nproc) check TESTS=""` to incrementally build the tools and test dependencies.
- **Bootstrap:** Run `./autogen.sh` only when autotools inputs change.
- Validate built-in modules with targeted configuration tests under `tests/`
  (e.g. `./tests/omfile-basic.sh`) and add regressions as needed.
- ompipe template coverage: `tests/ompipe-module-template.sh` checks module
  defaults, explicit action overrides, and legacy selector compatibility;
  `tests/yaml-ompipe-module-template.sh` checks the modern actions through YAML.
  `tests/ompipe-default-template.sh` checks the unchanged built-in default when
  no module template is configured.

## Metadata
- Shared metadata lives in `tools/MODULE_METADATA.json` so we do not duplicate
  YAML files beside each source.
- Keep the JSON array sorted by module name and update the `last_reviewed` field
  whenever support status, maturity, or contacts change.
- The default `primary_contact` points at the rsyslog GitHub Discussions and
  Issues queue; replace it with a specific maintainer only when a new owner is
  explicitly assigned.

## Documentation touchpoints
- Most of these modules have reference guides under
  `doc/source/configuration/modules/`. Update the relevant `.rst` files when you
  change defaults or add features.
- Cross-reference changes in `doc/ai/module_map.yaml` if the concurrency or
  locking guidance shifts.
