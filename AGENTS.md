# AGENTS.md – rsyslog Repository Agent Guide

This file defines guidelines and instructions for AI assistants (e.g., Codex, GitHub Copilot Workspace, ChatGPT agents) to understand and contribute effectively to the rsyslog codebase.

---

## 🧭 Repository Overview

- **Primary Language**: C  
- **Build System**: autotools (`autogen.sh`, `configure`, `make`)  
- **Modules**: Dynamically loaded from `modules/`  
- **Documentation**: Maintained in a separate repository ([rsyslog-doc](https://github.com/rsyslog/rsyslog-doc))  
- **Child Projects**:
  - [`rsyslog-docker`](https://github.com/rsyslog/rsyslog-docker): Provides prebuilt container environments for development and CI
- **Side Libraries** (each in its own repo within the rsyslog GitHub org):
  - [`liblognorm`](https://github.com/rsyslog/liblognorm)
  - [`librelp`](https://github.com/rsyslog/librelp)
  - [`libestr`](https://github.com/rsyslog/libestr)
  - [`libfastjson`](https://github.com/rsyslog/libfastjson)

---

## 🔄 Development Workflow

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

## 🏷️ Branch Naming Conventions

There are no strict naming rules, but these conventions are used frequently:

- For issue-based work: `i-<issue-number>` (e.g., `i-2245`)
- For features or refactoring: free-form is acceptable
- For AI-generated work: prefix the branch name with the AI tool name  
  (e.g., `gpt-i-2245-json-stats-export`)

---

## ✅ Coding Standards

- **Use tabs, not spaces** for indentation — enforced via CI
- Commit messages **must include all relevant information**, not just in the PR
- Favor **self-documenting code** over excessive inline comments
- Public functions should use Doxygen-style comments
- Modules must implement and register `modInit()` and `modExit()`

---

## 🧪 Testing & Validation

rsyslog uses a custom test driver and has complex build dependencies.

### 🧰 Test Framework

- All test definitions are located in the `/tests` directory.
- The test execution framework is implemented in `tests/diag.sh`.
- Tests are driven via `make check`, which wraps around `diag.sh`.

> ⚠️ Test execution is resource-intensive. Limit concurrency (`make check -j4` or less) to avoid unreliable results.

---

### ✅ Recommended Test Environment

Use prebuilt container environments provided by [`rsyslog-docker`](https://github.com/rsyslog/rsyslog-docker):

- They contain all required libraries and tools
- CI-like workflows can be reproduced locally
- Support full configuration flexibility (Valgrind, tsan, sanitizers, etc.)

> **If only a single test run is planned**, the best container to use is:  
> `rsyslog/rsyslog_dev_base_ubuntu:24.04`

GitHub Actions and other CI systems use these containers. Test execution typically takes 10–40 minutes per runner depending on the suite.

---

### ❗ Manual Setup (discouraged)

Minimum setup requires:

- Autotools toolchain: `autoconf`, `automake`, `libtool`, `make`, `gcc`
- Side libraries: `libestr`, `librelp`, `libfastjson`, `liblognorm` (must be installed or built manually)

Example commands:

```bash
./autogen.sh
./configure --enable-debug
make -j$(nproc)
make check
```

Use `make check -j2` or `-j4` max for reliability.

---

## 📚 Documentation

Main documentation is in a separate repo:  
🔗 https://github.com/rsyslog/rsyslog-doc

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

## 📋 Pull Request & Commit Guidelines

- PR title should be concise and informative  
- PR body should describe what changed, why, how it was tested, and any caveats
- **Commit messages must be complete** — assume the reader only has the git log
- Link relevant GitHub issues (e.g., `Closes #1234`)
- Use a single logical commit unless there's a clear need to split

---

## 💡 AI-Specific Hints

- The `modules/` directory contains dynamically loaded input/output plugins  
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

## 🤖 AI Agent Commit Convention

If you are an AI agent contributing code or documentation:

- Include a **commit footer tag** like:
  ```
  AI-Agent: Codex 2025-06
  ```
- Do not prefix commit messages with `AI:`; use the footer tag only.
- PR descriptions should clearly identify that they were generated or co-authored by an AI tool.
- Avoid generating multiple PRs for retries — reuse and update the original PR when possible.
- Follow the same **commit message policy** as human contributors: describe what changed, why, and how it was validated.

---

## 🔐 Privacy, Trust & Permissions

- AI agents **must not** push changes directly to user forks — always open PRs against `rsyslog/rsyslog`
- Do not install third-party dependencies unless explicitly approved
- PRs must pass standard CI and review checks
- All code **must be reviewed manually**; AI output is subject to full review

---

End of `AGENTS.md`
