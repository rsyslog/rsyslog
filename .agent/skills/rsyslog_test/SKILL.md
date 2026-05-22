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

For diagnostics emitted by rsyslog itself, follow `tests/AGENTS.md`: assert the
configured rsyslog output destination, usually testbench omfile output such as
`RSYSLOG_OUT_LOG`, after synchronized shutdown. Avoid rsyslogd stdout/stderr as
the oracle unless the test is specifically about process-level output or the
exception is documented in the test header.

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


### 10. Python Style Checks
For Python-only changes, use the repository style configuration in `setup.cfg`.
When `pycodestyle` is installed, run `devtools/format-python.sh
<changed-python-files>` to check changed Python files. Use
`devtools/format-python.sh --fix <changed-python-files>` only when you
intentionally want `autopep8` rewrites before the style check. If the tools are
missing in a local agent environment, suggest installing them (`sudo
apt-get install -y pycodestyle python3-autopep8` on Debian/Ubuntu) but do
not block unrelated build or test validation; `devtools/format-python.sh
--check-if-available ...` implements that optional behavior.

The pull-request workflow installs `pycodestyle` and intentionally checks only
changed Python files to avoid reintroducing full-tree style noise. It does not
run `autopep8`. Be cautious with legacy Python-2-style scripts: review
formatting changes that touch print statements, exception syntax, imports, or
line continuations before reporting the patch ready.

### 11. Optional PR-Local Linters
CodeFactor and CI provide central lint feedback, but local diff-scoped linter
runs are useful before pushing because they catch simple review noise early.
Run these only when the tools are installed; if a tool is missing, suggest the
install command and continue with normal build/test validation.

Fetch the base first when possible:

```bash
git fetch upstream main --prune
```

Recommended optional checks:

```bash
if command -v shellcheck >/dev/null 2>&1; then
  git diff -z --name-only --diff-filter=ACMR upstream/main...HEAD -- \
    '*.sh' | xargs -0 -r shellcheck -S warning
fi

if command -v hadolint >/dev/null 2>&1; then
  git diff -z --name-only --diff-filter=ACMR upstream/main...HEAD -- \
    '*Dockerfile*' 'Dockerfile' | xargs -0 -r hadolint
fi
```

For changed infrastructure/config files, run `trivy config` on the changed
paths or the smallest relevant directory when `trivy` is installed. For larger
PRs, run `jscpd` on changed source/test files when installed to spot accidental
copy/paste duplication. Treat duplication findings as review prompts, not
automatic blockers.

Do not include `cppcheck` in the routine local PR linter set unless a maintainer
explicitly asks for it; prior test runs showed too much low-value noise on this
code base.

### 12. Container Validation Escalation
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
