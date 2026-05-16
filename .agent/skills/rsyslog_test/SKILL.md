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

Use focused host-side tests while iterating on a specific behavior. For
PR-ready validation, use `rsyslog_local_container_testing` when local container
support is available; it catches CI-environment, compiler/toolchain, service
skip, and static-analyzer issues that host-side tests often cannot detect.

## Detailed Instructions

### 1. Direct Execution Rule
**NEVER** use `make check` during routine development. It is slow (runs 1000+ tests) and hides failure details.
- **Rule**: Run individual shell scripts directly from the `tests/` directory.
- **Benefit**: Immediate feedback and visible stdout/stderr.

### 2. Test Registration (tests/Makefile.am)
New tests MUST be registered using the "Define at Top, Distribute Unconditionally, Register Conditionally" pattern. This ensures `make distcheck` validity.

1.  **Define Variable**: Group your tests into a descriptive variable at the **top** of `tests/Makefile.am` (before the `if ENABLE_TESTBENCH` block).
    ```makefile
    TESTS_MYMODULE = \
        test1.sh \
        test2.sh
    ```
2.  **Unconditional Distribution**: Add it to `EXTRA_DIST` immediately after definition. **NEVER** add test scripts to `EXTRA_DIST` inside a conditional block.
    ```makefile
    EXTRA_DIST += $(TESTS_MYMODULE)
    ```
3.  **Conditional Registration**: Append the variable to the global `TESTS` list inside the appropriate `if ENABLE_...` block.
    ```makefile
    if ENABLE_MYMODULE_TESTS
    TESTS += $(TESTS_MYMODULE)
    endif
    ```
4.  **Serialization**: If tests must run in order, add `.log` dependencies:
    ```makefile
    test2.log: test1.log
    ```

### 3. Test Intent Documentation
Every new test MUST include a short comment near the top that states the exact
behavior, regression, or invariant being tested. When modifying an existing
test, update or add that comment if it is missing, stale, or too vague.

For timing, sampling, retry, concurrency, or negative-path tests, also document
the test oracle: what condition makes the test pass or fail, and why any
threshold or wait exists. Keep these comments focused on intent and semantics;
do not duplicate each shell command.

When changing a test, verify that the head comment still matches the actual
setup, stimulus, oracle, and pass/fail conditions after the edit; update it in
the same commit if it does not.

### 4. Using diag.sh Helpers
All tests include `tests/diag.sh` using the POSIX `.` command. You should use its standardized helpers:
- `cmp_exact`: Verify file content matches.
- `require_plugin`: Skip test if a module is not built.
- `command_deny`: Ensure a specific command fails.

### 5. Valgrind Testing
For memory leak and race condition detection:
- Use scripts ending in `-vg.sh`.
- These are wrappers that set `USE_VALGRIND=1` and include the base test using the `.` command.

### 6. Debugging Failed Tests
If a test fails, you can inspect logs by:
- Enabling debug: Uncomment `RSYSLOG_DEBUG` exports in `tests/diag.sh`.
- Disabling cleanup: Comment out `exit_test` at the end of the script.
- Output files: Look for `rstb_*.out.log` and `log`.

### 7. Integration Test Policy
Certain modules (Kafka, Elasticsearch, Journald) have heavy integration tests requiring external services.
- **Policy**: Skip these in restricted environments (like AI sandboxes) unless build-only validation is insufficient.
- Check module-specific `AGENTS.md` or `MODULE_METADATA.yaml`.

### 8. Memory Lifecycle Validation (Mental Audit)
Before committing C changes, agents SHOULD perform a self-audit of memory ownership and lifecycle.
- **Rule**: Run the `/audit` workflow. It orchestrates multiple specialized passes (Memory Guardian, Concurrency Architect) using the [Canned Prompts](../../../ai/).
- **Critical Patterns**:
  - **NULL Checks**: `es_str2cstr()` can return `NULL`. Every call MUST be followed by a `NULL` check.
  - **Macro Usage**: Prefer `CHKmalloc()` for allocations as it automatically handles the `NULL` check and jumps to `finalize_it`.
- **Focus**: Pay special attention to `RS_RET` error paths, `es_str2cstr()` checks, and `strdup` calls.

### 9. Mock Smoke Check (Fast Distcheck)
Whenever you add or rename test scripts, you MUST run a fast distribution check to ensure all files are correctly registered and invocable in a VPATH build. This avoids breaking CI for distribution-related issues.

**Command**:
```bash
make distcheck TEST_RUN_TYPE=MOCK-OK -j$(nproc)
```
- **Pattern**: This uses the `MOCK-OK` mode in `tests/diag.sh` to exit tests with success immediately, skipping the overhead of actual execution while still verifying shell script invocability and distribution completeness.

### 10. Container Validation Escalation
If container support is available and the change is intended for a PR, prefer
running `rsyslog_local_container_testing` before pushing. The local container
flow is often faster than discovering CI-only failures after the PR is opened,
especially for static-analyzer findings, compiler or dependency differences, generated
build state, and service-test relevance filtering.

Run the fast host-side checks first when debugging a narrow failure. Once the
patch is stable, escalate to the container validation skill for CI-style
confidence.

## Related Skills
- `rsyslog_build`: Required before running tests.
- `rsyslog_local_container_testing`: Preferred PR-ready validation path when
  local container support is available.
- `rsyslog_module`: Documentation on module-specific test dependencies.
