# AGENTS.md – Testbench subtree

This guide covers everything under `tests/`, including shell test cases,
helpers, suppressions, and supporting binaries. Pair it with
[`tests/README`](./README) for human- and CI-facing run instructions; this
document focuses on authoring guidance and the knobs that matter most to AI
agents.

## Purpose & scope
- The directory implements the Automake testbench that exercises rsyslog.
- Each `.sh` script is a standalone scenario that can be executed directly or
  through `make check`.
- Treat `tests/` as the single recursive test-owning subtree. Keep new unit
  test sources under `tests/unit/` or similar subfolders, but register and run
  them from `tests/Makefile.am` instead of creating another recursive
  `tests/.../Makefile.am` harness. Recursive Automake propagates
  `make check TESTS=...` into every subdirectory, so splitting test ownership
  across multiple subdirs creates fragile name collisions and selection
  failures.
- Use this guide together with the top-level `AGENTS.md` and the component
  guide that matches the module you are testing.

## Flake-prone test antipatterns

These patterns come from actual deflake fixes. They are not banned in every
case, but new or changed tests must either avoid them or explain why they are
safe in that specific test. Before adding or heavily changing tests, run the
advisory scan:

```bash
devtools/check-test-antipatterns.sh tests/<changed-test>.sh
```

The tool uses `rg` when available and falls back to `grep`, so it can run in
minimal containers. Findings are review prompts, not automatic blockers.

- **Port preselection with `get_free_port`**: selecting a free port before the
  listener binds is racy. Another process can take the port in the gap. Prefer
  listener `port="0"` plus a port file, or helper-owned readiness where the file
  is written only after `listen(2)` succeeds.
- **Fixed sleeps as synchronization**: `sleep 1`, `msleep 3000`, and similar
  waits usually encode host-speed assumptions. Prefer explicit readiness or
  completion oracles such as port files, `wait_file_lines`, `wait_queueempty`,
  stats counters, or imdiag waits. If a sleep intentionally creates retry
  pressure, document that timing role and the success/failure oracle.
- **Readiness files written too early**: a port or ready file must mean the
  consumer can proceed now, not merely that setup has started or a port number
  was chosen.
- **Negative-path tests without a deterministic oracle**: auth failures,
  retries, disconnects, timeouts, and shutdown-abort paths must wait for a
  specific state, diagnostic, queue condition, or process result instead of
  assuming it happened after a delay.
- **CPU tick, runtime, or timeout thresholds without rationale**: thresholds
  are acceptable only when the test header explains what the number proves and
  why it is high enough under loaded CI runners.
- **Background helpers without lifecycle control**: backgrounded servers,
  clients, or probes need deterministic readiness and cleanup. Prefer existing
  testbench helpers; if custom plumbing is required, make ownership and cleanup
  explicit.
- **Daemonized shutdown tests without a final rsyslogd oracle**: shell ``wait``
  cannot observe the real daemon process exit status after rsyslogd forks. For
  daemonized tests that care about crashes or clean shutdown, use
  ``set_proper_termination_file`` before ``generate_conf`` and
  ``check_proper_termination`` after ``wait_shutdown``. Do not use helper
  cleanup or observer processes as the clean-shutdown oracle. ``timeoutGuard``
  remains mandatory hang protection and must not be removed, disabled, or
  weakened to make such tests pass; if timeoutGuard aborts rsyslogd, it must
  preserve the termination marker with ``status=error`` and useful diagnostics.
- **Queue tests assuming immediate drain or shutdown ordering**: use
  queue-specific synchronization where possible. Do not assume that input
  completion, shutdown start, or a fixed delay means all queued messages reached
  the tested stage.
- **Shared external state**: fixed filenames, spool directories, service names,
  ports, topics, databases, and state files can collide under parallel
  `make check`. Use dynamic names derived from the testbench instance where
  possible.
- **Scope-reducing deflake fixes**: do not make a test pass by removing the
  behavior, race window, or invariant it was meant to exercise. If the oracle or
  setup changes, refresh the head comment and verify the original behavior is
  still covered.

## Writing & updating tests
- Base new shell tests on existing ones; include `. $srcdir/diag.sh` to gain the
  helper functions (timeouts, Valgrind integration, rsyslogd launch helpers).
- Every new or changed test must document the exact behavior, regression, or
  invariant it covers. Add or refresh a short comment near the top of the test
  when the current intent is missing, stale, or vague. For timing, retry,
  sampling, concurrency, or negative-path tests, also explain the oracle: what
  proves success or failure, and why any wait or threshold exists.
  When changing a test, verify that the head comment still matches the actual
  setup, stimulus, oracle, and pass/fail conditions after the edit; update it in
  the same commit if it does not.
- For diagnostics emitted by rsyslog itself, prefer asserting the configured
  rsyslog output destination, usually testbench omfile output such as
  `RSYSLOG_OUT_LOG`, after synchronized shutdown. Do not use rsyslogd
  stdout/stderr as the oracle
  unless the behavior being tested is specifically process-level stdout/stderr
  emission, startup before configuration is usable, or another documented case
  where the message cannot pass through rsyslog's normal output path. Explain
  such exceptions in the test header.
- Prefer harness helpers such as `content_check`, `content_count_check`,
  `custom_content_check`, `check_not_present`, `cmp_exact`, `command_deny`, and
  `require_plugin` over ad-hoc shell to keep diagnostics uniform. In
  particular, use `content_check "needle" "$file"` instead of hand-written
  `grep ... || { cat "$file"; error_exit 1; }` blocks when asserting log or
  output content.
- Fix practical matches from `devtools/check-test-antipatterns.sh` when
  touching a test. If a match is intentional, explain the deterministic oracle
  in the test header instead of leaving the pattern as unexplained timing or
  concurrency folklore.
- **Config format coverage**: When a module parameter or config object is tested
  via RainerScript, add a companion YAML test (or extend an existing
  `yaml-<area>-*.sh`) that exercises the same parameter.  Both frontends share
  the same backend, but bugs can exist in the YAML parser alone.  Name YAML
  tests `yaml-<area>-<what>.sh` and use a `.yaml` extension for the config
  fixture in `tests/testsuites/` so the YAML loader is triggered automatically.
  Register the test in `tests/Makefile.am` under the same module conditionals
  as its RainerScript counterpart.  See the `rsyslog_config` skill for full
  conventions.
- Name Valgrind-enabled wrappers with the `-vg.sh` suffix and toggle Valgrind by
  exporting `USE_VALGRIND` before including the non-`vg` script using the `.`
  command. Use `tests/timereported-utc-vg.sh` as the reference layout: it
  includes the base scenario using the `.` command instead of copying it and
  demonstrates paring back emitted messages when the underlying test is
  slow—especially important under Valgrind. Older wrappers still duplicate
  logic; prefer the modern pattern when touching them.
- Put auxiliary binaries next to their scripts (e.g. `*.c` programs compiled via
  the Automake harness) and list them in `tests/Makefile.am`. Unit-test sources
  may live in `tests/unit/`, but the owning harness remains `tests/Makefile.am`.
- Keep long-lived configuration snippets in `tests/testsuites/` and reuse them
  instead of copying large config blocks into multiple scripts.
- Document new environment flags or helper functions inside `diag.sh` so other
  tests can discover them. Mention the addition in `tests/README` if operators
  should know about the new knob.

## Running tests locally
- Build rsyslog first using the efficient incremental command:
  ```bash
  make -j$(nproc) check TESTS=""
  ```
  This ensures the testbench can load freshly built binaries and modules. If the `Makefile` is missing, see "Step 2" in the top-level `AGENTS.md`.
- Execute individual scenarios directly for quick feedback
  (`./tests/imfile-basic.sh`).
- Use `make check TESTS='script.sh'` when you need Automake logging,
  parallelisation control, or to exercise the Valgrind wrappers.
- For unit binaries registered in `tests/Makefile.am`, use
  `make check TESTS='binary_name'` from the repository root so Automake builds
  the required runtime libraries before entering `tests/`.

### Multi-Module Test Guards
When adding a test that requires multiple modules (e.g., `imtcp` AND `imptcp`), you **MUST** wrap the test definition in `tests/Makefile.am` with significantly separate conditionals for **ALL** required modules. Do not assume one implies the others.
**Example**:
```makefile
if ENABLE_IMTCP
if ENABLE_IMPTCP
TESTS += multi_module_test.sh
endif
endif
```
Failing to do this breaks the build on systems where only one of the modules is enabled.
- Remove stale `.log`/`.trs` files before re-running a flaky test to avoid
  Automake caching previous outcomes.
- For configuration validation changes, run `./tests/validation-run.sh` to
  confirm both failure and success paths.

## Debugging & environment control
- `tests/diag.sh` documents environment variables such as `SUDO`,
  `USE_VALGRIND`, `RSYSLOG_DEBUG`, and timeout overrides; prefer these knobs over
  ad-hoc `sleep` loops. `tests/README` mirrors the operator-facing knobs so
  tooling and human docs stay aligned.
- Use `USE_GDB=1 make <test>.log` to pause execution and attach a debugger as
  described in `tests/README`.
- Keep suppression files (e.g. `*.supp`) current when adding new Valgrind noise;
  failing to do so will cause CI false positives.

### Enabling Debug Output

To enable rsyslog debug logging for a test, temporarily uncomment these lines in `tests/diag.sh` (around lines 88-89):

```bash
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="log"
```

This creates a `log` file in the tests directory with detailed execution traces.

**Important:** Remember to re-comment these lines after debugging to avoid cluttering test output.

### Preventing Test Cleanup for Inspection

To examine test output files after a test runs, temporarily comment out `exit_test` at the end of the test script:

```bash
# exit_test  # Temporarily disabled to inspect logs
```

This preserves:
- `rstb_*.out.log` - The actual test output
- `rstb_*.conf` - The generated rsyslog configuration
- `log` - Debug log (if enabled)
- `rstb_*.input*` - Test input files

### Example Debugging Workflow

1. **Enable debug output** in `diag.sh`:
   ```bash
   # Uncomment lines 88-89
   export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
   export RSYSLOG_DEBUGLOG="log"
   ```

2. **Disable cleanup** in your test script:
   ```bash
   # Comment the exit_test line
   #exit_test
   ```

3. **Run the test**:
   ```bash
   cd tests
   ./mmsnareparse-trailing-extradata.sh
   ```

4. **Examine output**:
   ```bash
   # Check actual output vs expected
   cat rstb_*.out.log
   
   # Search debug log for specific patterns
   grep "extradata_section" log
   grep "Truncated trailing" log
   ```

5. **Restore test environment**:
   - Re-comment debug exports in `diag.sh`
   - Uncomment `exit_test` in your test script
   - Clean up test artifacts: `rm -f rstb_* log`

### Understanding Test Output

When a test fails with `content_check`, the error shows:
```
FAIL: content_check failed to find "expected content"
FILE "rstb_*.out.log" content:
     1  actual line 1
     2  actual line 2
```

This helps identify:
- What the test expected vs what was produced
- Whether the module parsed the message correctly
- If fields are populated as expected

## YAML-only test mode

`generate_conf --yaml-only` creates a pure-YAML preamble (no RainerScript preamble)
that is used as the rsyslogd startup configuration directly.  Use it when a test
must validate YAML-loader behaviour or when no RainerScript is desired.

### How it works
- `generate_conf --yaml-only [instance]` writes `${TESTCONF_NM}[instance].yaml`
  containing `version: 2`, `global:`, and `testbench_modules:` (imdiag setup).
  `testbench_modules:` is a YAML key understood by rsyslogd as an alias for
  `modules:` and is reserved for testbench infrastructure — it avoids any
  conflict with the test's own `modules:` section.
- Tests add their own `modules:` section (and `inputs:`, `rulesets:`, etc.)
  via `add_yaml_conf`.
- `add_yaml_conf 'fragment' [instance]` appends arbitrary YAML to the same file.
- `add_yaml_imdiag_input [instance]` is a historical compatibility helper.
  imdiag startup detection is configured by `generate_conf --yaml-only` via
  module-scoped testbench parameters, so new tests should not call it.
- `startup_common` detects `RSYSLOG_YAML_ONLY=1` and passes the `.yaml` file
  to rsyslogd instead of the usual `.conf` file.

### Limitations
The following testbench features are **not available** in yaml-only mode:

| Feature | Reason | Workaround |
|---------|--------|-----------|
| Legacy `$` directives | Legacy syntax is not parsed by the YAML loader | Use v2 RainerScript (`module()`, `input()`) or YAML keys instead |

> **Note**: Startup detection uses the imdiag port file in both RainerScript and
> yaml-only modes.  The `.started` marker file mechanism has been removed; the
> imdiag port file is the sole startup signal in all modes.

### Example test structure
```bash
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf --yaml-only
# Test-specific modules in their own modules: section (testbench_modules: is in preamble)
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf "  - type: imtcp"
add_yaml_conf "    port: \"0\""
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\""
add_yaml_conf "    ruleset: main"
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" file=\"${RSYSLOG_OUT_LOG}\")"
startup
# ... test body ...
exit_test
```

### Naming and registration
- Name yaml-only tests `yaml-<area>-yamlonly.sh` (or append `-yamlonly` to an
  existing test name) so they are easy to find.
- Register them in `tests/Makefile.am` under `TESTS_LIBYAML`.

## Coordination

- When adding tests for a plugin or runtime subsystem, mention them in the
  component’s `AGENTS.md` so future authors know smoke coverage exists.
- Update `KNOWN_ISSUES` or module metadata if a test encodes a known bug or a
  skipped scenario.
- If a change requires additional docker services or fixtures, document setup
  steps in `tests/CI/README` (or create it) and link from the relevant module
  guide.
