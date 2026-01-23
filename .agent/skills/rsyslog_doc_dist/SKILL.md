---
name: rsyslog_doc_dist
description: Ensures doc/Makefile.am stays in sync with changes to documentation files.
triggers:
  - doc/source/**/*.rst
condition: "on create, move, or delete" # Do not trigger for content-only edits
---

# rsyslog_doc_dist

This skill ensures that all documentation files are correctly registered for distribution in the build system.

## Quick Start

1.  **Add File**: If you add a `*.rst` file in `doc/source/`, add it to `EXTRA_DIST` in `doc/Makefile.am`.
2.  **Move/Rename**: If you move or rename a file, update the corresponding path in `doc/Makefile.am`.
3.  **Remove**: If you delete a file, remove its entry from `doc/Makefile.am`.

## Detailed Instructions

### 1. The EXTRA_DIST Guard
The rsyslog distribution tarball is created using `make dist`. For documentation to be included, every source file must be listed in the `EXTRA_DIST` variable within `doc/Makefile.am`.

### 2. Synchronization Rule
Whenever you perform a filesystem operation on documentation:
- **Addition**: Insert the new path (relative to `doc/`) into the `EXTRA_DIST` list. Try to maintain the existing logical grouping (e.g., `source/configuration/modules/`).
- **Renaming/Moving**: Locate the old path in `doc/Makefile.am` and replace it with the new one.
- **Deletion**: Locate and remove the entry to avoid build failures during `make dist`.

### 3. Extended Verification (Automated)
For changes involving file additions, moves, or deletions, you MUST run the extended distribution check. This ensures that the documentation is correctly packaged in the source tarball and can be built from it.

**Command**:
```bash
bash .agent/skills/rsyslog_doc_dist/scripts/check-doc-dist.sh
```

This script will:
1. Parse the project version.
2. Run `make dist`.
3. Unpack the distribution in a temporary directory.
4. Run `autoreconf -fvi && ./configure.sh` (special case).
5. Verify `make html` in the distribution.
6. Clean up temporary files.

## Related Skills
- `rsyslog_doc`: For documentation content and metadata standards.
- `rsyslog_build`: For verifying that the build system (including `Makefile.am`) is still functional after changes.
