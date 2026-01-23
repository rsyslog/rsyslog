---
name: rsyslog_build
description: Handles environment setup and high-performance incremental building for rsyslog.
---

# rsyslog_build

This skill standardizes the environment setup and build process for the rsyslog project. It ensures that agents use the most efficient build commands and handle bootstrap logic correctly.

## Quick Start

1.  **Setup Environment**: Run `.agent/skills/rsyslog_build/scripts/setup.sh` to install dependencies.
2.  **Efficient Build**: Use `make -j$(nproc) check TESTS=""` for incremental builds.
3.  **Bootstrap**: Use `./autogen.sh --enable-debug [options]` if `Makefile` is missing or build configuration changes.

## Detailed Instructions

### 1. Environment Setup
The project requires a set of development libraries (C toolchain, libcurl, libfastjson, etc.).
- Use the provided script: `bash .agent/skills/rsyslog_build/scripts/setup.sh`
- This script wraps `devtools/codex-setup.sh` and ensures all CI-standard tools are present.

### 2. Bootstrapping (autogen.sh)
You MUST run `./autogen.sh` in the following cases:
- After a fresh checkout (no `Makefile` exists).
- If you modify `configure.ac`, any `Makefile.am`, or files under `m4/`.
- If you need to enable/disable specific modules (e.g., `--enable-imkafka`).

**Example**:
```bash
./autogen.sh --enable-debug --enable-testbench --enable-imdiag --enable-omstdout
```

### 3. High-Performance Build
Always prefer the incremental build-only command. It builds the core and all test dependencies without the overhead of running the full test suite.

**Command**:
```bash
make -j$(nproc) check TESTS=""
```

### 4. Runtime & Library Considerations
When modifying `runtime/` or exported symbols:
- Ensure the library version script (if touched) is consistent.
- Incremental builds handle `-M../runtime/.libs` correctly for dynamic loading.

## Related Skills
- `rsyslog_test`: For running validations after a successful build.
- `rsyslog_module`: For module-specific build flags.
