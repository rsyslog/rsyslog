# AGENTS.md – rsyslog Repository Agent Guide

This file defines guidelines and instructions for AI assistants (e.g., Codex, GitHub Copilot Workspace, ChatGPT agents) to understand and contribute effectively to the rsyslog codebase.

## Agent Quick Start: The "Happy Path"

Follow these three steps for a typical development task. This workflow is the recommended starting point for any new session.

### Step 1: Set Up the Environment (If Needed)

For some agents, like Jules, the development environment is often Debian/Ubuntu-based. If you are in such an environment and need to install dependencies, the following command provides a complete set for building and testing.

**Warning:** Only run this command if you are on a Debian-based system (like Ubuntu) and have `sudo` privileges. Do not run this in an unknown CI or containerized environment, as it may cause unintended changes.

```bash
# Optional: For Debian/Ubuntu-based environments
sudo apt-get update && sudo apt-get install -y \
    autoconf autoconf-archive automake autotools-dev \
    bison flex gcc \
    libcurl4-gnutls-dev libdbi-dev libgcrypt20-dev \
    libglib2.0-dev libgnutls28-dev \
    libtool libtool-bin libzstd-dev make \
    libestr-dev python3-docutils libfastjson-dev \
    librelp-dev liblognorm-dev libaprutil1-dev libcivetweb-dev \
    valgrind clang-format
```

### Step 2: Build the Project

Build the project with a recommended set of `./configure` options that enables the testbench and a common set of modules suitable for most development and testing tasks.

```bash
./autogen.sh
./configure --enable-testbench --enable-imdiag --enable-omstdout --enable-mmsnareparse --enable-omotel --enable-imhttp
make -j$(nproc)
```

**Note:** If `autogen.sh` has been run before, you only need to run it again if `configure.ac` or `Makefile.am` files have changed.

### Step 3: Run Tests

Run a relevant test to verify your changes. The testbench allows test scripts to be run directly. `imtcp-basic.sh` serves as a good general-purpose smoke test.

```bash
./tests/imtcp-basic.sh
```

## Repository Overview

  - **Primary Language**: C
  - **Build System**: autotools (`autogen.sh`, `configure`, `make`)
  - **Modules**: Dynamically loaded from `plugins/`
  - **Contrib Modules**: Community-contributed under `contrib/`
  - **Contributions**: Additional modules and features are placed in `contrib/`, which contains community-contributed plugins not actively maintained by the core rsyslog team. These are retained in `contrib/` even if adopted later, to avoid disruptions in dependent software.
  - **Documentation**: Maintained in the doc/ subdirectory
  - **AI module map**: `doc/ai/module_map.yaml` (per-module paths & locking hints)
  - **docker definitions**: Maintained in the packaging/docker/ subdirectory
  - **Side Libraries** (each in its own repo within the rsyslog GitHub org):
      - [`liblognorm`](https://github.com/rsyslog/liblognorm)
      - [`librelp`](https://github.com/rsyslog/librelp)
      - [`libestr`](https://github.com/rsyslog/libestr)
      - [`libfastjson`](https://github.com/rsyslog/libfastjson): A fork of libfastjson by the rsyslog project, optimized for speed.
        This library is used by multiple external projects.

-----

## Quick links for agents

  - **Documentation subtree guide:** [`doc/AGENTS.md`](./doc/AGENTS.md)
  - **Coding practices reference (patterns & antipatterns for AI seeding):**
    [`doc/source/development/coding_practices.rst`](./doc/source/development/coding_practices.rst)
  - **Core plugin subtree guide:** [`plugins/AGENTS.md`](./plugins/AGENTS.md)
  - **Contrib module subtree guide:** [`contrib/AGENTS.md`](./contrib/AGENTS.md)
  - **Built-in tools subtree guide:** [`tools/AGENTS.md`](./tools/AGENTS.md)
  - **Runtime core guide:** [`runtime/AGENTS.md`](./runtime/AGENTS.md)
  - **Testbench guide:** [`tests/AGENTS.md`](./tests/AGENTS.md)
  - **Inline comment conventions:** [`COMMENTING_STYLE.md`](./COMMENTING_STYLE.md)
  - **Module author checklist:** [`MODULE_AUTHOR_CHECKLIST.md`](./MODULE_AUTHOR_CHECKLIST.md)
  - **Developer overview:** [`DEVELOPING.md`](./DEVELOPING.md)
  - **Commit prompt template:** [`ai/rsyslog_commit_assistant/base_prompt.txt`](./ai/rsyslog_commit_assistant/base_prompt.txt)
  - **Doc builder prompt template:** [`ai/rsyslog_code_doc_builder/base_prompt.txt`](./ai/rsyslog_code_doc_builder/base_prompt.txt)

Use these jump points together with this file to locate the workflow and
component notes that apply to your task.

-----

## Agent Chat Keywords

The following chat codewords instruct AI assistants to perform standardized actions in this repository.

### `FINISH`

When the user says the codeword "FINISH", do the following:

- Perform a final review of all proposed code changes for correctness and style before concluding the session.

### `SUMMARIZE`

When the user says the codeword "SUMMARIZE", do the following:

- Create and print the following summaries in the Agent chat, each in a copy-ready TEXTBOX field:
  - A summary for the pull request
  - A summary for a squashed commit message

### `SETUP`

When the user says the codeword "SETUP", do the following:

- Follow the instructions in the "Step 1: Set Up the Environment" section of the "Agent Quick Start" guide at the top of this file to install all necessary development dependencies.

### `BUILD [configure-options]`

When the user says the codeword "BUILD" optionally followed by configure options, do the following:

1. **Generate the autotools build system** (if not already done):
   ```bash
   ./autogen.sh
   ```

2. **Configure the project**:
   - If configure options are provided after "BUILD", use them:
     ```bash
     ./configure [user-provided-options]
     ```
   - If no options are provided, use the default testbench configuration:
     ```bash
     ./configure --enable-testbench --enable-imdiag --enable-omstdout
     ```

3. **Build the project**:
   ```bash
   make -j$(nproc)
   ```

Examples:
- `BUILD` - Uses default testbench configuration
- `BUILD --enable-testbench --enable-mmsnareparse` - Custom configuration with mmsnareparse module
- `BUILD --enable-testbench --enable-imdiag --enable-omstdout --enable-mmsnareparse --enable-omotel` - Multiple modules

### `TEST [test-script-names]`

When the user says the codeword "TEST" optionally followed by test script names, do the following:

1. **Ensure the project is built** (if not already built, run BUILD first)

2. **Run tests**:
   - If test script names are provided after "TEST", run those specific tests:
     ```bash
     ./tests/<test-script-name>.sh
     ```
     For multiple tests, run each one:
     ```bash
     ./tests/<test-script-1>.sh
     ./tests/<test-script-2>.sh
     ```
   - If no test names are provided, run the default smoke test:
     ```bash
     ./tests/imtcp-basic.sh
     ```

Examples:
- `TEST` - Runs the default smoke test (`imtcp-basic.sh`)
- `TEST mmsnareparse-sysmon.sh` - Runs the mmsnareparse-sysmon test
- `TEST mmsnareparse-sysmon.sh mmsnareparse-trailing-extradata.sh` - Runs multiple specific tests

Note: Tests are run directly (not via `make check`) to provide unfiltered stdout/stderr output.

-----

## Priming a fresh AI session

When starting a new AI-assisted coding session (for example after a PR merges or
the workspace is reset):

1. Share this repository-level guide and the relevant subtree `AGENTS.md`
   files (`plugins/`, `contrib/`, `doc/`, `tools/`, etc.) so the agent absorbs
   the area-specific build and testing workflows.
2. Provide the module metadata (`MODULE_METADATA.yaml` or `tools/MODULE_METADATA.json`)
   for components being modified so ownership, support channels, and maturity
   are clear.
3. Supply the commit assistant or doc builder prompt that matches the task type
   (see the quick links above) to keep commit messages and documentation edits
   consistent.
4. Include any recent design or review notes that are not yet in the repository
   so the agent understands outstanding context.

These steps mirror how the existing sandbox is configured and make it more
likely that rebuild/bootstrap reminders (such as running `./autogen.sh` before
the first compile) are followed.

-----

## Automated Formatting Normalization Strategy

We treat formatting as a normalization step, not a developer-side constraint.
AI agents should follow this process:

1.  Canonical formatting via clang-format
    Use the Google-style base with 4-space indentation in .clang-format.

2.  Helper-based normalization
    Run devtools/format-code.sh to run clang-format and potential helper scripts.

3.  Developer freedom & accessibility
    Local formatting is unrestricted; only normalized output matters.

4.  CI enforcement & maintainer assist
    CI checks formatting; maintainers may edit PRs directly.

5.  Git blame hygiene
    Formatting-only commits listed in .git-blame-ignore-revs.

AI Agent Note: run devtools/format-code.sh as the final formatting step before commit.

-----

## Development Workflow

### Base Repository

- URL: https://github.com/rsyslog/rsyslog
- **Default base branch: `main`**
  > The `main` branch is now the canonical base for all development.
  > Some older references to `master` may still exist in documentation
  > or tooling and will be updated as needed.

### Contributor Workflow

1.  Fork the repository (for personal development)
2.  Create a feature/fix branch
3.  Push changes to your fork
4.  Open a **pull request directly into `rsyslog/rsyslog:main`**

> **Important**: AI-generated PRs must target the `rsyslog/rsyslog` repository directly.

-----

## Branch Naming Conventions

There are no strict naming rules, but these conventions are used frequently:

  - For issue-based work: `i-<issue-number>` (e.g., `i-2245`)
  - For features or refactoring: free-form is acceptable
  - For AI-generated work: prefix the branch name with the AI tool name
    (e.g., `gpt-i-2245-json-stats-export`)

-----

## Coding Standards

  - Commit messages **must include all relevant information**, not just in the PR
  - Commit message titles **must not exceed 62 characters**
  - commit message text must be plain US ASCII, line length must not exceed 72 characters
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

-----

## Defensive Coding and Assertions

Use `assert()` to signal "impossible" states to Static Analyzers and AI agents. Whenever feasible and low-complexity, follow with a defensive `if` check to prevent production crashes. See [Defensive Coding Practice](doc/source/development/coding_practices/defensive_coding.rst) for full details.

- **Mandatory**: `assert()` for invariants (allows SA/AI to reason about code).
- **Recommended**: Defensive `if` check (optional if fallback logic is excessively complex).
- **Prohibited**: `__builtin_unreachable()` (causes Undefined Behavior).

-----

## Editor & Formatting Configuration

The repository includes:

  - `.editorconfig`: Editor-agnostic indent, whitespace, EOL, and file-type rules.
  - Project `.vimrc`/`.exrc`: Vim settings when `set exrc secure` is enabled.
  - `.clang-format`: Canonical C/C++ style; run `clang-format -i -style=file`.
  - `devtools/format-code.sh`: Runs clang-format plus helper fixups.

Editors and IDEs with EditorConfig support (VS Code, JetBrains, Sublime, Vim, Emacs)
will automatically apply these rules.

-----

## Build & Test Expectations

Whenever `.c` or `.h` files are modified, a build should be performed using `make`.
If new functionality is introduced, at least a basic test should be created and run.

### Generating the autotools build system

The `configure` script and `Makefile.in` files are **not** stored in git. After a
fresh checkout—or any time `configure.ac`, `Makefile.am`, or macros under `m4/`
change—you **must** run:

```bash
./autogen.sh
```

This bootstraps autotools, downloads any required macros, and generates
`configure`. Skipping this step is the most common reason for messages such as
`configure: error: cannot find install-sh, install.sh, or shtool` or `make:
*** No targets specified and no makefile found`. If a cleanup removed the
generated files (e.g., `git clean -xfd`), re-run `./autogen.sh` before
configuring again.

If `autogen.sh` fails, run `./devtools/codex-setup.sh` first to install the
toolchain dependencies inside the sandbox, then retry `./autogen.sh`.

### Configure & build

If possible, agents should:

  - Build the project using `./configure` and `make`
  - Run an individual test using the instructions below
  - After building, run `./tests/imtcp-basic.sh` as a smoke test unless another test is more appropriate

> In restricted environments, a build may not be possible. In such cases, ensure the
> generated code is clear and well-commented to aid review.

-----

## Testing & Validation

All test definitions live under the `tests/` directory and are driven by the `tests/diag.sh` framework. Continuous-integration jobs still execute the full suite via `make check`, but **AI agents should stick to direct test scripts** unless a reviewer explicitly requests a CI reproduction. Direct invocation keeps stdout/stderr visible and avoids the 10+ minute runtime of the harness.

Avoiding the harness matters because `make check`:

  - Wraps tests in a harness that hides stdout/stderr on failure
  - Requires parsing `tests/test-suite.log` for details
  - Consumes significant resources on large suites

Instead, AI agents should invoke individual test scripts directly. This yields unfiltered output and immediate feedback, without the CI harness. The `diag.sh` framework now builds any required test support automatically, so there is **no longer** a need for a separate “build core components” step.

-----

## Documentation Requirements

When introducing new configuration parameters, features, or significant behavior changes, you **must** update the user-facing documentation in the `doc/` subtree.

1.  **Locate the relevant guide**: Most module documentation is in `doc/source/configuration/modules/<module>.rst`.
2.  **Update parameter references**: If adding a parameter, create or update the corresponding file in `doc/source/reference/parameters/` and include it in the module's `.rst` file.
3.  **Cross-link**: Ensure new documentation is discoverable from the module's main page and appropriate `index.rst`.
4.  **Validate**: If possible, run `./doc/tools/build-doc-linux.sh --clean --format html` to catch Sphinx errors.

-----

### Running Individual Tests (AI-Agent Best Practice)

1.  **Configure the project** (once per session):
    ```bash
    ./configure --enable-imdiag --enable-testbench
    ```
2.  **Invoke your test**:
    ```bash
    ./tests/<test-script>.sh
    ```
    For example:
    ```bash
    ./tests/manytcp-too-few-tls-vg.sh > /tmp/test.log && tail -n20 /tmp/test.log
    ```
3.  **Why this works**
      - Each test script transparently finds and loads the test harness
      - You get unfiltered stdout/stderr without any CI wrapper
      - No manual `cd` or log-file parsing required

-----

### Full Test Suite (CI-Only)

For continuous-integration pipelines or when you explicitly need to validate the entire suite, use the autotools harness:

```bash
./configure --enable-imdiag --enable-testbench
make check
```

  - To limit parallelism (avoid flaky failures), pass `-j2` or `-j4` to `make`.
  - If a failure occurs, inspect `tests/test-suite.log` for detailed diagnostics.
  - Note in your report why a full harness run was necessary so reviewers understand the extra runtime.

-----

### Test Environment

Human developers can replicate CI conditions using the official container images available on **Docker Hub**. For single-test runs, we recommend `rsyslog/rsyslog_dev_base_ubuntu:24.04`. It is **recommended** that AI agents use the standard workflow within their existing environment to avoid potential complications, but they may use container images if necessary to reproduce a specific environment.

-----


-----

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

### `fmpcre`

  - Buildable: Yes when `libpcre3-dev` (or equivalent) is installed
  - Testable: Yes, simple regression test `ffmpcre-basic.sh` exercises `pcre_match()`

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

-----

## AI-Specific Hints

  - The `plugins/` directory contains dynamically loaded input/output plugins
  - `contrib/` contains external contributions (e.g., plugins) that are not core-maintained
  - `statsobj.c` implements the statistics interface
  - Documentation resides in the monorepo’s doc/ directory
  - Discovery order for AI agents: start with this file, follow the per-tree
    `AGENTS.md` (for docs, `doc/AGENTS.md`), then ingest the coding practices
    reference at `doc/source/development/coding_practices.rst` to prime RAG
    seeding before planning or reviews.
  - You may reference `rsyslog-docker` for dev/test environment setup
  - Side libraries are external GitHub repos, not subdirectories

  - **Shell Script Documentation**
    Use shdoc-style comments (`##`, `###`) in new and updated Bash scripts to enable automatic Markdown extraction. Many existing scripts lack these; it's **strongly recommended** to add them when modifying or creating scripts.

When generating or editing code, prefer:

  - Clean modular design
  - Compatibility and backward safety
  - Updating structured comments (e.g., Doxygen for C code)

-----

## AI Agent Commit Convention

If you are an AI agent contributing code or documentation:

  - Use the same rich commit message text as your PR description.
  - Avoid generating multiple PRs for retries — reuse and update the original PR when possible.
  - Follow the same **commit message policy** as human contributors:
      - Describe **what changed** and **why** (as far as known to the agent).
      - Note any impact on existing versions or behaviors (especially for bug fixes).
  - Commit message descriptions should clearly identify that they were generated or co-authored by an AI tool.
  - Include a line in the commit footer like `With the help of AI-Agents: <agent-name>`
  - **When crafting commit messages, you must use the canonical commit-message base prompt** located at `ai/rsyslog_commit_assistant/base_prompt.txt`. This template ensures the final commit adheres to the project's formatting rules: a title of **62 characters or less** and body lines wrapped at **72 characters**.
  - **Commit-first:** ensure the substance is in the commit body (not only the PR). If needed, amend before opening the PR (`git commit --amend`).

-----

## Privacy, Trust & Permissions

  - AI agents **must not** push changes directly to user forks — always open PRs against `rsyslog/rsyslog`
  - Do not install third-party dependencies unless explicitly approved
  - PRs must pass standard CI and review checks
  - All code **must be reviewed manually**; AI output is subject to full review

-----

## Quickstart for AI coding agents (v8 concurrency & state)

**Read these first:**
* [`DEVELOPING.md`](./DEVELOPING.md) — v8 worker model & locking rules
* [`MODULE_AUTHOR_CHECKLIST.md`](./MODULE_AUTHOR_CHECKLIST.md) — one-screen checklist
* [doc/ai/module_map.yaml](./doc/ai/module_map.yaml) — seed list of modules, paths, and known locking needs

**Rules you must not break**
1. The framework may run **multiple workers per action**.
2. `wrkrInstanceData_t` (WID) is **per-worker**; never share it.
3. Shared mutable state lives in **pData** (per-action) and **must be protected**
   by the module (mutex/rwlock). Do **not** rely on `mutAction` for this.
4. **Inherently serial resources** (e.g., a shared stream) must be serialized
   inside the module via a mutex in **pData**.
5. **Direct queues** do not remove the need to serialize serial resources.

**Common agent tasks**
* Consult `doc/ai/module_map.yaml` to understand module paths and known locking.
* Add a “Concurrency & Locking” block at the top of output modules.
* Ensure serial modules guard stream/flush with a **pData** mutex.
* For modules with a library I/O thread (e.g., Proton), verify read/write locks
  are taken on **all** callback paths.
