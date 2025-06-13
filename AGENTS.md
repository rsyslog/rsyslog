# AGENTS.md – rsyslog Repository Agent Guide

This file defines guidelines and instructions for AI assistants (e.g., Codex, GitHub Copilot Workspace, ChatGPT agents) to understand and contribute effectively to the rsyslog codebase.

---

## Repository Overview

- **Primary Language**: C  
- **Build System**: autotools (`autogen.sh`, `configure`, `make`)  
- **Modules**: Dynamically loaded from `modules/`  
- **Contributions**: Additional modules and features are placed in `contrib/`, which contains community-contributed plugins not actively maintained by the core rsyslog team. These are retained in `contrib/` even if adopted later, to avoid disruptions in dependent software.  
- **Documentation**: Maintained in a separate repository ([rsyslog-doc](https://github.com/rsyslog/rsyslog-doc))  
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


- All test definitions are located in the `/tests` directory.
- The test execution framework is implemented in `tests/diag.sh`.
- Tests are driven via `make check`, which wraps around `diag.sh`.

Each test file in `tests/` is a standalone bash script that is executed independently. These scripts typically begin by sourcing `tests/diag.sh`, which provides a shared library of functions and utilities for test execution and validation.

> Test execution is resource-intensive. Limit concurrency (`make check -j4` or less) to avoid unreliable results.

To run `make check`, you **must configure with `--enable-imdiag --enable-testbench`**. The test suite includes many test cases and may run for 10+ minutes. To save time, prefer targeting individual tests by name. Tests with filenames containing "basic" are typically good candidates for quick validation.

- **Best Practice**: When specifying individual tests, use the test basename only (no directory prefix and omit the `.sh` suffix). For example:
  ```bash
  make check TESTS=rscript_re_extract
  ```

### Test Framework

Some extended tests involving external components (e.g., daemons or network services) may be flaky due to timing conditions. When a test fails but passes on re-run, it's usually a nondeterministic issue. Such behavior should be reviewed but does not always indicate a defect.

The `imdiag` module exists specifically to support the test framework and is used for command and information passing during tests.

When a test fails, **read the referenced `tests/test-suite.log`** file to analyze failure details.

---

### Recommended Test Environment

Use prebuilt container environments provided by [`rsyslog-docker`](https://github.com/rsyslog/rsyslog-docker):

- They contain all required libraries and tools
- CI-like workflows can be reproduced locally
- Support full configuration flexibility (Valgrind, tsan, sanitizers, etc.)

> If only a single test run is planned, the best container to use is:  
> `rsyslog/rsyslog_dev_base_ubuntu:24.04`

GitHub Actions and other CI systems use these containers. Test execution typically takes 10–40 minutes per runner depending on the suite.

---

### Manual Setup (discouraged)

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

## Documentation

Main documentation is in a separate repo:  
https://github.com/rsyslog/rsyslog-doc

To build locally:

```bash
cd rsyslog-doc
pip install -r requirements.txt
make html
```

If a feature changes user-facing behavior or configuration:
- Update the appropriate section in `rsyslog-doc`
- Link it in your PR and commit message

---

## Pull Request & Commit Guidelines

- PR title should be concise and informative  
- PR body should describe what changed, why, how it was tested, and any caveats
- **Commit messages must be complete** — assume the reader only has the git log
- **Commit title must not exceed 70 characters**
- Link relevant GitHub issues using the full URL (e.g., `https://github.com/rsyslog/rsyslog/issues/1234`)
- Use a single logical commit unless there's a clear need to split

---

## AI-Specific Hints

- The `modules/` directory contains dynamically loaded input/output plugins  
- `contrib/` contains external contributions (e.g. plugins) that are not core-maintained  
- `statsobj.c` implements the statistics interface  
- `imhttp` provides HTTP input and optionally Prometheus metrics  
- Use `doc/` only for legacy inline docs — modern documentation lives in `rsyslog-doc`
- You may reference `rsyslog-docker` for dev/test environment setup
- Side libraries are external GitHub repos, not subdirectories

When generating or editing code, prefer:
- Clean modular design  
- Compatibility and backward safety  
- Updating structured comments (e.g., Doxygen)

---

## AI Agent Commit Convention

If you are an AI agent contributing code or documentation:

- Include a **commit footer tag** like:
  ```
  AI-Agent: Codex 2025-06
  ```
- PR descriptions should clearly identify that they were generated or co-authored by an AI tool.
- Avoid generating multiple PRs for retries — reuse and update the original PR when possible.
- Follow the same **commit message policy** as human contributors: describe what changed, why, and how it was validated.

---

## Privacy, Trust & Permissions

- AI agents **must not** push changes directly to user forks — always open PRs against `rsyslog/rsyslog`
- Do not install third-party dependencies unless explicitly approved
- PRs must pass standard CI and review checks
- All code **must be reviewed manually**; AI output is subject to full review

---

End of `AGENTS.md`
