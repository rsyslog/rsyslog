# AGENTS.md – Documentation subtree

This guide applies to everything under `doc/`.

## Purpose & scope
- The `doc/` tree contains all **Sphinx documentation sources**, helper tools, and the **AI knowledge base** under `doc/ai/`.
- Use this file together with the top-level `AGENTS.md` or `CONTRIBUTING.md` instructions.

## Editing guidelines
- Prefer **reStructuredText (`*.rst`)** for content.  
  Markdown files are reserved for meta-docs such as `/doc/ai/` and other authoring guides.
- Follow existing heading levels and section names from `doc/README.md`.
- Cross-link new pages from the appropriate `index.rst` (or local `.. toctree::`) so they appear in navigation.
- Keep the `source/development/coding_practices.rst` page discoverable; it is
  the canonical source for patterns and antipatterns that AI agents must
  ingest.
- When touching shared style guidance, also review `doc/STRATEGY.md`.

### For AI-driven documentation work
- Prime new contributors or agents with the base prompt at  
  **`ai/rsyslog_code_doc_builder/base_prompt.txt`**.  
  This defines the model’s role, tone, and default workflow.
- Use the supplemental **knowledge base** in `doc/ai/`:
  - `authoring_guidelines.md` — required blocks, tone, and section order.
  - `mermaid_rules.md` — syntax rules (blank line after directive, quoted node labels, `<br>` for line breaks).
  - `templates/` — standard concept/tutorial RST templates.
  - `terminology.md` — canonical rsyslog vocabulary.
- Prime code-generation agents with the `source/development/coding_practices.rst`
  page so RAG pipelines can inject the right patterns and antipatterns into
  their context.
- Keep discoverability explicit for AI agents: this file is linked from the
  top-level `AGENTS.md`, and `coding_practices.rst` sits in the development
  toctree. When you move or rename that page, update the links here and in the
  base prompts so automated seeding does not break.
- Every new page must include anchors, meta, and summary blocks per `authoring_guidelines.md`.

## Build & validation
- Run `./doc/tools/build-doc-linux.sh --clean --format html` after changes to catch Sphinx errors early.
- For quick linting, use `make -C doc html` (uses the repo’s virtualenv if present).
- Verify Mermaid diagrams render correctly; invalid syntax halts the build.
- Documentation-only commits generally do **not** require the full C test suite.

## Commit messaging
- Summarize the **reader impact** (new topic, restructure, typo fix, etc.) in the commit body.
- Include the `AI-Agent: ChatGPT` trailer as requested by repository guidelines.
- If a change updates or regenerates the KB, mention the **KB version** in the message.

## Coordination
- When editing module-specific docs, consult `doc/ai/module_map.yaml` for component ownership.
- Mention any **locking or runtime considerations** in the relevant module page.
- If the change alters common terms (e.g., *log pipeline*), update both the glossary and `/doc/ai/terminology.md`.

## Quick reference
| Task | Location |
|------|-----------|
| Base prompt (AI agent seed) | `ai/rsyslog_code_doc_builder/base_prompt.txt` |
| AI knowledge base | `doc/ai/` |
| Mermaid rules | `doc/ai/mermaid_rules.md` |
| Authoring guide | `doc/ai/authoring_guidelines.md` |
| Concept/tutorial templates | `doc/ai/templates/` |
| Build scripts | `doc/tools/` |
| Strategy and style | `doc/STRATEGY.md` |

