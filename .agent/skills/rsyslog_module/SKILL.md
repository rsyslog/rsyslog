---
name: rsyslog_module
description: Encodes technical requirements for rsyslog modules, including concurrency, metadata, and initialization.
---

# rsyslog_module

This skill captures the essential technical patterns for authoring and maintaining rsyslog modules (plugins/contrib).

## Quick Start

1.  **Locking**: Follow the "Belt and Suspenders" rule (`assert()` + `if`).
2.  **State**: `pData` (shared) vs `WID` (per-worker).
3.  **Boilerplate**: Use `BEGINmodInit`, `CODESTARTmodInit`, etc. 
    - *Resource*: See [Common Snippets](./resources/snippets.md) for boilerplate code.

## Detailed Instructions

### 1. Concurrency & Locking
Rsyslog v8 has a high-concurrency worker model.
- **Shared State (`pData`)**: Mutable state shared across workers MUST be protected by a mutex in `pData`.
- **Per-Worker State (`WID`)**: Never share `wrkrInstanceData_t`.
- **Belt and Suspenders**:
  ```c
  assert(pData != NULL);
  if (pData == NULL) {
      // Handle error gracefully
  }
  ```
- **Headers**: Every output module should have a "Concurrency & Locking" header block.

### 2. Module Lifecycle
Every module must implement and register standard entry points:
- `modInit()`: Initialize static data and registry interfaces.
- `modExit()`: Finalize and cleanup.
- `beginTransaction()` / `commitTransaction()`: For efficient batch-based output.

### 3. Metadata Consistency
- **Location**: `MODULE_METADATA.yaml` in the module directory.
- **Synchronization**: Keep `doc/ai/module_map.yaml` in sync with locking and concurrency changes.

### 4. Build Configuration
- Update `plugins/Makefile.am` and `configure.ac` when adding new modules.
- **Test Registration**: Follow the "Define at Top, Distribute Unconditionally, Register Conditionally" pattern in `tests/Makefile.am`. See the `rsyslog_test` skill for details. This is critical for `make distcheck` validity.
- Use `MODULE_TYPE(eMOD_OUT)` and other macros from `runtime/module-template.h`.

## Related Skills
- `rsyslog_build`: For compiling the module.
- `rsyslog_test`: For creating module-specific smoke tests.
- `rsyslog_doc`: For documentation requirements.
