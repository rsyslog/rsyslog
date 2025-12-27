# doc/ai/AGENTS.md – Documentation Subtree Agent Guide

This file defines specific guidelines for AI assistants working on the documentation subtree (`doc/`).

## Quick Links

- **Authoring Guidelines:** [`doc/ai/authoring_guidelines.md`](./authoring_guidelines.md)
- **Structure and Paths:** [`doc/ai/structure_and_paths.md`](./structure_and_paths.md)
- **Terminology:** [`doc/ai/terminology.md`](./terminology.md)
- **Doc Assistant Prompt:** [`ai/rsyslog_doc_assistant/base_prompt.txt`](../../ai/rsyslog_doc_assistant/base_prompt.txt)

## Critical Requirements for Documentation

All documentation files (especially module documentation) must strictly adhere to the following metadata requirements to ensure high-quality AI ingestion and user experience:

1.  **Metadata Block (`.. meta::`)**: Every file must start with a metadata block containing `:description:` and `:keywords:`.
2.  **Summary Slice (`.. summary-start/end`)**: Every file (and included parameter file) must have a concise summary enclosed in these markers. This is critical for RAG (Retrieval-Augmented Generation) systems.
3.  **Anchors**: Use consistent, explicit anchors for sections and parameters as defined in the authoring guidelines.

**Before editing any documentation:**
- Read the **Doc Assistant Prompt** linked above. It serves as the canonical checklist for structure and style.
- Consult `authoring_guidelines.md` for specific RST syntax rules.

## Common Tasks

### Updating Module Documentation
- Ensure the module page (`doc/source/configuration/modules/<module>.rst`) has the required `.. meta::` block.
- Verify that the "Purpose" or introductory section is wrapped in `.. summary-start` and `.. summary-end`.
- Check that parameter tables use the `include` directive to pull in parameter details from `doc/source/reference/parameters/`.

### Creating New Documentation
- Start by creating a checklist based on the **Doc Assistant Prompt**.
- Use existing core modules (like `imfile`) as a template, but strictly verify against the current guidelines.

## Building Documentation
(Instructions on how to build docs locally if applicable, e.g., referencing `doc/tools/build-doc-linux.sh`)
