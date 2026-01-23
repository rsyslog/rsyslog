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

### 3. Verification
You can verify synchronization by checking if the number of `.rst` files in `doc/source` matches the count in `doc/Makefile.am` (adjusting for non-rst files in `EXTRA_DIST`).

**Command to find unregistered files**:
```bash
# Run from the doc/ directory
find source -name "*.rst" | sort > /tmp/rst_files
grep "source/.*\.rst" Makefile.am | sed 's/^[ \t]*//;s/[ \t]*\\$//' | sort > /tmp/makefile_files
diff /tmp/rst_files /tmp/makefile_files
```

## Related Skills
- `rsyslog_doc`: For documentation content and metadata standards.
- `rsyslog_build`: For verifying that the build system (including `Makefile.am`) is still functional after changes.
