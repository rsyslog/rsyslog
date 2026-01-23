---
name: rsyslog_test
description: Standardizes testing and validation for rsyslog using the diag.sh framework.
---

# rsyslog_test

This skill provides guidelines and tools for running tests efficiently. It emphasizes direct test script execution to avoid the overhead and opacity of the full `make check` harness.

## Quick Start

1.  **Build First**: Ensure the project is built (use `rsyslog_build`).
2.  **Run Specific Test**: `./tests/<test-name>.sh`
3.  **Run with Valgrind**: `./tests/<test-name>-vg.sh`

## Detailed Instructions

### 1. Direct Execution Rule
**NEVER** use `make check` during routine development. It is slow (runs 1000+ tests) and hides failure details.
- **Rule**: Run individual shell scripts directly from the `tests/` directory.
- **Benefit**: Immediate feedback and visible stdout/stderr.

### 2. Using diag.sh Helpers
All tests source `tests/diag.sh`. You should use its standardized helpers:
- `cmp_exact`: Verify file content matches.
- `require_plugin`: Skip test if a module is not built.
- `command_deny`: Ensure a specific command fails.

### 3. Valgrind Testing
For memory leak and race condition detection:
- Use scripts ending in `-vg.sh`.
- These are wrappers that set `USE_VALGRIND=1` and source the base test.

### 4. Debugging Failed Tests
If a test fails, you can inspect logs by:
- Enabling debug: Uncomment `RSYSLOG_DEBUG` exports in `tests/diag.sh`.
- Disabling cleanup: Comment out `exit_test` at the end of the script.
- Output files: Look for `rstb_*.out.log` and `log`.

### 5. Integration Test Policy
Certain modules (Kafka, Elasticsearch, Journald) have heavy integration tests requiring external services.
- **Policy**: Skip these in restricted environments (like AI sandboxes) unless build-only validation is insufficient.
- Check module-specific `AGENTS.md` or `MODULE_METADATA.yaml`.

### 6. Memory Lifecycle Validation (Mental Audit)
Before committing C changes, agents SHOULD perform a self-audit of memory ownership and lifecycle.
- **Rule**: Use the [Memory Lifecycle Prompt](../../ai/rsyslog_memory_auditor/base_prompt.txt) to review your diff.
- **Critical Patterns**: 
  - **NULL Checks**: `es_str2cstr()` can return `NULL`. Every call MUST be followed by a `NULL` check.
  - **Macro Usage**: Prefer `CHKmalloc()` for allocations as it automatically handles the `NULL` check and jumps to `finalize_it`.
- **Focus**: Pay special attention to `RS_RET` error paths and `strdup` calls in configuration parsing.

## Related Skills
- `rsyslog_build`: Required before running tests.
- `rsyslog_module`: Documentation on module-specific test dependencies.
