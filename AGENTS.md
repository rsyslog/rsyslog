# AGENTS.md – rsyslog Repository Agent Guide

This file defines guidelines and instructions for AI assistants (e.g., Codex, GitHub Copilot Workspace, ChatGPT agents) to understand and contribute effectively to the rsyslog codebase.

---

## Repository Overview

- **Primary Language**: C
- **Build System**: autotools (`autogen.sh`, `configure`, `make`)
- **Modules**: Dynamically loaded from `modules/`
- **Contributions**: Additional modules and features are placed in `contrib/`, which contains community-contributed plugins not actively maintained by the core rsyslog team. These are retained in `contrib/` even if adopted later, to avoid disruptions in dependent software.
- **Documentation**: Maintained in the doc/ subdirectory
- **Child Projects**:
  - [`rsyslog-docker`](https://github.com/rsyslog/rsyslog-docker): Provides prebuilt container environments for development and CI
- **Side Libraries** (each in its own repo within the rsyslog GitHub org):
  - [`liblognorm`](https://github.com/rsyslog/liblognorm)
  - [`librelp`](https://github.com/rsyslog/librelp)
  - [`libestr`](https://github.com/rsyslog/libestr)
  - [`libfastjson`](https://github.com/rsyslog/libfastjson): A fork of libfastjson by the rsyslog project, optimized for speed. This library is used by multiple external projects.

---

## Development Workflow

### Base Repository
- URL: https://github.com/rsyslog/rsyslog
- **Default base branch: `master`**
  > For technical reasons, `master` is still the default branch. Numerous scripts and CI workflows rely on this name.

### Contributor Workflow
1. Fork the repository (for personal development)
2. Create a feature/fix branch
3. Push changes to your fork
4. Open a **pull request directly into `rsyslog/rsyslog:master`**

> **Important**: AI-generated PRs must target the `rsyslog/rsyslog` repository directly.

---

## Branch Naming Conventions

There are no strict naming rules, but these conventions are used frequently:

- For issue-based work: `i-<issue-number>` (e.g., `i-2245`)
- For features or refactoring: free-form is acceptable
- For AI-generated work: prefix the branch name with the AI tool name
  (e.g., `gpt-i-2245-json-stats-export`)

---

## Coding Standards

- **Use tabs, not spaces** for indentation — enforced via CI
- Commit messages **must include all relevant information**, not just in the PR
- Commit message titles **must not exceed 70 characters**
- commit message text must be plain US ASCII, line length must not exceed 86 characters
- When referencing GitHub issues, use the **full GitHub URL** to assist in `git log`-based reviews
- Favor **self-documenting code** over excessive inline comments
- Public functions should use Doxygen-style comments
- See `COMMENTING_STYLE.md` for detailed Doxygen guidelines
- Modules must implement and register `modInit()` and `modExit()`

When fixing compiler warnings like `stringop-overread`, explain in the commit message:
- Why the warning occurred
- What part of the code was changed
- How the fix prevents undefined behavior or aligns with compiler expectations
- Optionally link: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wstringop-overread

### Code Style Rules Enforced by `devtools/check-whitespace`:

- Lines **must end with a single LF** (no missing or extra newlines)
- **Maximum line length**: 120 columns (tab width = 8 spaces)
- **No leading space** at beginning of line (except for ` *` in comments)
- **No trailing whitespace** at line end
- **DOS CRLF format is not permitted**

### Structured Error Handling Convention

- Use `ABORT_FINALIZE(code);` to both set `iRet = code` and jump to `finalize`.
- Use `FINALIZE;` when exiting with the current `iRet` value.
- Use `DEFiRet` at the beginning of a function to define `iRet`.
- Use the label `finalize_it:` to mark the exception return point.
- Return using `RETiRet`.
- Use `CHKiRet()` to check a function's return and jump to `finalize_it` on failure.
- Use `CHKmalloc()` and similar macros to verify allocations or calls.
- `localRet` is a temporary `rsRetVal` for intermediate error evaluation.

---

## Testing & Validation

All test definitions live under the `tests/` directory and are driven by the `tests/diag.sh` framework. In CI, `make check` remains the canonical way to run the full suite; **AI agents should avoid invoking `make check` by default**, because:

- It wraps all tests in a harness that hides stdout/stderr on failure
- It requires parsing `tests/test-suite.log` for details
- It can take 10+ minutes and consume significant resources

Instead, AI agents should invoke individual test scripts directly. This yields unfiltered output and immediate feedback, without the CI harness. The `diag.sh` framework now builds any required test support automatically, so there is **no longer** a need for a separate “build core components” step.

---

### Running Individual Tests (AI-Agent Best Practice)

1. **Configure the project** (once per session):
   ```bash
   ./configure --enable-imdiag --enable-testbench
   ```
2. **Invoke your test**:
   ```bash
   ./tests/<test-script>.sh
   ```
   For example:
   ```bash
   ./tests/manytcp-too-few-tls-vg.sh > /tmp/test.log && tail -n20 /tmp/test.log
   ```
3. **Why this works**
   - Each test script transparently finds and loads the test harness
   - You get unfiltered stdout/stderr without any CI wrapper
   - No manual `cd` or log-file parsing required

---

### Full Test Suite (CI-Only)

For continuous-integration pipelines or when you need to validate the entire suite, use the autotools harness:

```bash
./configure --enable-imdiag --enable-testbench
make check
```

- To limit parallelism (avoid flaky failures), pass `-j2` or `-j4` to `make`.
- If a failure occurs, inspect `tests/test-suite.log` for detailed diagnostics.

---

### Test Environment

Human developers may reproduce CI conditions using the official container images from [`rsyslog/rsyslog-docker`](https://github.com/rsyslog/rsyslog-docker). For single-test runs, `rsyslog/rsyslog_dev_base_ubuntu:24.04` is recommended—but **AI agents should not attempt to pull or run these images**. They must use the standard configure + direct-test workflow inside their existing container.

---

## Manual Setup (discouraged)

Minimum setup requires:

- Autotools toolchain: `autoconf`, `automake`, `libtool`, `make`, `gcc`
- Side libraries: `libestr`, `librelp`, `libfastjson`, `liblognorm` (must be installed or built manually)

Example commands:

```bash
./autogen.sh
./configure --enable-debug --enable-testbench
make -j$(nproc)
make check
```

Use `make check -j2` or `-j4` max for reliability.

---

### Codex Build Environment Setup

Codex-based agents must only perform this setup **if a compile or test is required** (e.g., not for review-only tasks):

- Run this setup script to install core dependencies:
  ```bash
  ./devtools/codex-setup.sh
  ```
- Then, always run:
  ```bash
  ./autogen.sh
  ```

This ensures Codex can build core components even in constrained environments. Skipping setup when not needed (e.g., code review) saves significant execution time.

---

## Module-Specific Capabilities

### `omelasticsearch`
- Buildable: Yes, even in minimal environments
- Depends on: `libcurl`
- Testable: No. Tests require a running Elasticsearch instance and are skipped in Codex or constrained environments

### `imjournal`
- Buildable: Yes
- Depends on: `libsystemd`
- Testable: No. Requires journald-related libraries and a systemd journal service context not present in the Codex container

### `imkafka` and `omkafka`
- Depends on: `librdkafka` (plus `liblz4` when linking statically)

### `omhiredis` and `imhiredis`
- Depends on: `hiredis`; `imhiredis` also needs `libevent`

### `ommongodb`
- Depends on: `libmongoc-1.0`

### `omamqp1` and `omazureeventhubs`
- Depends on: `libqpid-proton` (Azure module additionally needs `libqpid-proton-proactor`)

### `imhttp`
- Depends on: `civetweb` and `apr-util`

### `imdocker`
- Depends on: `libcurl` (>= 7.40.0)

### `impcap`
- Depends on: `libpcap`

### `imczmq` and `omczmq`
- Depends on: `libczmq` (>= 4.0.0)

### `omrabbitmq`
- Depends on: `librabbitmq` (>= 0.2.0)

### `omdtls` and `imdtls`
- Depends on: `openssl` (>= 1.0.2 for output, >= 1.1.0 for input)

### `omhttp`
- Depends on: `libcurl`

### `omhttpfs`
- Depends on: `libcurl`

### `mmnormalize`
- Depends on: `liblognorm` (>= 2.0.3)

### `mmkubernetes`
- Depends on: `libcurl` and `liblognorm` (>= 2.0.3)

### `mmgrok`
- Depends on: `grok` and `glib-2.0`

### `mmdblookup`
- Depends on: `libmaxminddb` (dummy module built if absent)

### `omlibdbi`
- Depends on: `libdbi`

### `ommysql`
- Depends on: `mysqlclient` via `mysql_config`

### `ompgsql`
- Depends on: `libpq` via `pg_config`

### `omsnmp`
- Depends on: `net-snmp`

### `omgssapi`
- Depends on: `gssapi` library

---

## AI-Specific Hints

- The `modules/` directory contains dynamically loaded input/output plugins
- `contrib/` contains external contributions (e.g., plugins) that are not core-maintained
- `statsobj.c` implements the statistics interface
- Documentation resides in the monorepo’s doc/ directory
- You may reference `rsyslog-docker` for dev/test environment setup
- Side libraries are external GitHub repos, not subdirectories

- **Shell Script Documentation**
  Use shdoc-style comments (`##`, `###`) in new and updated Bash scripts to enable automatic Markdown extraction. Many existing scripts lack these; it's **strongly recommended** to add them when modifying or creating scripts.

- **Final Style Check**
  Always run `python3 devtools/rsyslog_stylecheck.py` before committing. Fix any style errors and re-run until it passes.

When generating or editing code, prefer:
- Clean modular design
- Compatibility and backward safety
- Updating structured comments (e.g., Doxygen for C code)

---

## AI Agent Commit Convention

If you are an AI agent contributing code or documentation:

- Use the same rich commit message text as your PR description.
- Avoid generating multiple PRs for retries — reuse and update the original PR when possible.
- Follow the same **commit message policy** as human contributors:
  - Describe **what changed** and **why** (as far as known to the agent).
  - Note any impact on existing versions or behaviors (especially for bug fixes).
- Commit message descriptions should clearly identify that they were generated or co-authored by an AI tool.
- Include a **commit footer tag** like "AI-Agent: Codex"

---

## Privacy, Trust & Permissions

- AI agents **must not** push changes directly to user forks — always open PRs against `rsyslog/rsyslog`
- Do not install third-party dependencies unless explicitly approved
- PRs must pass standard CI and review checks
- All code **must be reviewed manually**; AI output is subject to full review
