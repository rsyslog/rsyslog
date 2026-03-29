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
- Use this guide together with the top-level `AGENTS.md` and the component
  guide that matches the module you are testing.

## Writing & updating tests
- Base new shell tests on existing ones; include `. $srcdir/diag.sh` to gain the
  helper functions (timeouts, Valgrind integration, rsyslogd launch helpers).
- Prefer harness helpers such as `cmp_exact`, `command_deny`, and
  `require_plugin` over ad-hoc shell to keep diagnostics uniform.
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
  exporting `USE_VALGRIND` before sourcing the non-`vg` script. Use
  `tests/timereported-utc-vg.sh` as the reference layout: it sources the base
  scenario instead of copying it and demonstrates paring back emitted messages
  when the underlying test is slow—especially important under Valgrind. Older
  wrappers still duplicate logic; prefer the modern pattern when touching them.
- Put auxiliary binaries next to their scripts (e.g. `*.c` programs compiled via
  the Automake harness) and list them in `tests/Makefile.am`.
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
  containing `version: 2`, `global:`, `mainqueue:`, and `modules:` (imdiag).
  The `modules:` sequence is intentionally left open — no `inputs:` section is
  written by the preamble.
- Tests append additional module entries as **sequence continuation items**
  (2-space indent, `  - load: ...`, no top-level `modules:` key) so YAML parsers
  see a single, well-formed `modules:` list.  The sequence closes naturally when
  the test adds a zero-indent key such as `inputs:`.
- `add_yaml_conf 'fragment' [instance]` appends arbitrary YAML to the same file.
- `add_yaml_imdiag_input [instance]` appends the imdiag input entry
  (`  - type: imdiag / port: "0"`) inside an already-opened `inputs:` block.
  **Tests must call this** (inside their `inputs:` section) so that startup
  detection via the imdiag port file works correctly.
- `startup_common` detects `RSYSLOG_YAML_ONLY=1` and passes the `.yaml` file
  to rsyslogd instead of the usual `.conf` file.

### Limitations
The following testbench features are **not available** in yaml-only mode:

| Feature | Reason | Workaround |
|---------|--------|-----------|
| `.started` marker file | The syslogtag-based filter rule requires RainerScript/legacy syntax | `wait_startup` requires the imdiag port file (not just the PID file) in yaml-only mode, confirming config loaded and inputs are active; it fast-fails if rsyslog exits before the port file appears |

> **Note**: Queue and input timeout settings (`$MainmsgQueueTimeout*`,
> `inputs.timeout.shutdown`, `default.action.queue.*`) are configured via imdiag
> module parameters in the config preamble (`module(load="imdiag" mainmsgqueuetimeoutshutdown=... )`).
> This is config-time, requires no post-startup round-trips, and works identically
> for both RainerScript and yaml-only modes.  Tests that need a non-default value
> must set the relevant `RSTB_*` variable **before** calling `generate_conf`.

### Example test structure
```bash
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf --yaml-only
# Continue the modules: sequence opened by the preamble (2-space indent, no modules: key)
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_imdiag_input           # required for startup detection
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
