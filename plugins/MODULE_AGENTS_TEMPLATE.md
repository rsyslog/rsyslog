# AGENTS.md – <module name>

## Module overview
- Purpose of the module and key user documentation links.
- Support status & maturity (mirror `MODULE_METADATA.yaml`).

## Build & dependencies
- Configure flags to enable the module (e.g. `--enable-foo`).
- Library/tool prerequisites and how to install them inside the sandbox.
- Reminders about rerunning `./autogen.sh` or `configure` when touching build
  inputs.

## Local testing
- Smoke tests (`make foo-basic.log`) and longer scenarios.
- Helpers or environment variables required to provision external services.
- Notes about test runtime or resource requirements.

## Diagnostics & troubleshooting
- Key log messages, stats counters, or generated files that validate success.
- Typical failure signatures and quick triage steps.

## Cross-component coordination
- Shared helpers or configuration formats that must stay aligned.
- Other modules or docs to update when behavior changes.

## Metadata & housekeeping
- Remind contributors to update `MODULE_METADATA.yaml`.
- Call out any templates or automation that need refreshing alongside code
  changes.
