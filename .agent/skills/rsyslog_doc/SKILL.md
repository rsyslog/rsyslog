name: rsyslog_doc
description: Guidelines for maintaining structured, RAG-optimized documentation and module metadata.
triggers:
  - doc/source/**/*.rst
---

# rsyslog_doc

This skill ensures that all documentation is consistent, discoverable, and optimized for both human readers and AI ingestion systems.

## Quick Start

1.  **Metadata Block**: Every `.rst` must have a `.. meta::` block.
2.  **Summary Slices**: Wrap intros in `.. summary-start` and `.. summary-end`.
3.  **Cross-Link**: Update `index.rst` and `doc/ai/module_map.yaml`.

## Detailed Instructions

### 1. Structured Requirements
Every documentation page must include:
- **Meta Block**:
  ```rst
  .. meta::
     :description: Brief description for SEO and RAG.
     :keywords: rsyslog, module, config, ...
  ```
- **Summary Slices**: Essential for RAG (Retrieval-Augmented Generation).
  ```rst
  .. summary-start
  Concise summary of what this module/feature does.
  .. summary-end
  ```

---
> [!IMPORTANT]
> **Trigger Side-Effect**: If you add, move, or remove any `.rst` file, YOU MUST follow the [`rsyslog_doc_dist`](../rsyslog_doc_dist/SKILL.md) skill to update `doc/Makefile.am` and run the **extended distribution check**.

### 2. Module Documentation
- **Parameters**: Use the `include` directive to pull parameter details from `doc/source/reference/parameters/`.
- **Anchors**: Use explicit anchors (e.g., `.. _parameter_name:`) for consistent linking.
- **Templates**: Reference `doc/ai/templates/template-module.rst`.

### 3. Metadata Files
- **Plugins/Contrib**: Maintain `MODULE_METADATA.yaml` in the module directory.
- **Built-in Tools**: Update `tools/MODULE_METADATA.json`.
- **Required Keys**: `support_status`, `maturity_level`, `primary_contact`, `last_reviewed`.

### 4. Validation
- **Build Docs**: Run `./doc/tools/build-doc-linux.sh --clean --format html`.
- **json-formatter**: Run `make -j16 json-formatter` to update the RAG knowledge base.
- **Mermaid**: Ensure Mermaid diagrams have a blank line after the directive and quoted labels.

### 5. Style & Tone
- Follow the **Doc Assistant Prompt**: `ai/rsyslog_doc_assistant/base_prompt.txt`.
- Use canonical terminology from `doc/ai/terminology.md`.

## Related Skills
- `rsyslog_module`: For technical details to include in docs.
- `rsyslog_commit`: For doc-only commit message rules.
