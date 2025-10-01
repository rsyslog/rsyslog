# AGENTS.md – Documentation subtree

This guide applies to everything under `doc/`.

## Purpose & scope
- The `doc/` tree contains the Sphinx documentation sources plus helper tools.
- Use this file together with the top-level `AGENTS.md` instructions.

## Editing guidelines
- Prefer reStructuredText (`*.rst`) for content; Markdown files exist for historical reasons.
- Keep headings consistent with the existing `doc/README.md` conventions.
- Cross-link new material from `doc/README.md` or the relevant `index.rst` so it is discoverable.
- When touching shared style guidance, update `doc/STRATEGY.md` if needed.
- Prime new contributors with the doc-builder base prompt at
  `ai/rsyslog_code_doc_builder/base_prompt.txt` when scoping AI-driven documentation
  work.

## Build & validation
- After content changes, run `./doc/tools/build-doc-linux.sh --clean --format html` locally when possible to catch Sphinx errors.
- For quick linting without rebuilding everything, run `make -C doc html` (uses the repo's virtualenv if already set up).
- Document-only changes generally do not require running the full C test suite.

## Commit messaging
- Summarize the reader impact (new topics, reorganized structure, typo fixes) in the commit body.
- Include the `AI-Agent: ChatGPT` trailer as requested by the repository guidelines.

## Coordination
- If a change affects module-specific docs, check `doc/ai/module_map.yaml` for component details and mention any locking or
  runtime considerations in the documentation where relevant.
