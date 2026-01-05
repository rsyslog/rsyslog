# AGENTS.md – omruleset compatibility module

## Module overview
- Historical helper that forwards a message to another ruleset.
- Replaced by the RainerScript `call` statement; retained only for backward compatibility.
- User documentation: `doc/source/configuration/modules/omruleset.rst`.
- Support status: core-supported. Maturity: deprecated.

## Build & dependencies
- Built automatically when plugins are enabled; no extra configure flags or external dependencies.
- **Efficient Build:** Use `make -j$(nproc) check TESTS=""`.
- **Bootstrap:** Run `./autogen.sh` only when autotools inputs change.

## Local testing
- There is no standalone test suite for `omruleset`. Building rsyslog with `make` is sufficient validation.
- When refactoring, rely on configuration-level tests that exercise ruleset chaining rather than new module-specific scripts.

## Diagnostics & troubleshooting
- Failures typically stem from queueing semantics or recursion in user configurations. Encourage migrating to the `call` statement where possible.
- Review `doc/source/configuration/modules/omruleset.rst` for historical caveats around queue saturation and message loss.

## Cross-component coordination
- Document any behavior-affecting changes in `doc/source/configuration/modules/omruleset.rst` and update migration notes that point to the `call` statement.
- Audit related examples in `doc/source/configuration/modules/omfile.rst` and `doc/source/rainerscript/` if you touch shared queue guidance.

## Metadata & housekeeping
- Keep `plugins/omruleset/MODULE_METADATA.yaml` marked as `deprecated` and refresh the review date when compatibility fixes land.
- If the module is ever removed, update the documentation and release notes to highlight the required migration.
