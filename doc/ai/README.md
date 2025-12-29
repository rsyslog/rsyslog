# rsyslog AI Knowledge Base (KB)

**Purpose:** A compact, upload-ready knowledge base that reflects the *current* rsyslog docs structure and rules.

**Audience:** Contributors and AI assistants generating/maintaining docs.

## What's here

| File | Purpose |
|------|---------|
| `structure_and_paths.md` | Directory layout and naming conventions |
| `authoring_guidelines.md` | Required blocks, tone, section order |
| `mermaid_rules.md` | Diagram syntax rules |
| `terminology.md` | Canonical rsyslog vocabulary |
| `chunking_and_embeddings.md` | RAG extraction schema and chunk structure |
| `crosslinking_and_nav.md` | Navigation and cross-reference patterns |
| `drift_monitoring.md` | Detecting doc/code drift |
| `module_map.yaml` | Module paths and locking hints |
| `templates/` | RST templates for concept, tutorial, and module pages |

## RAG Knowledge Base

The documentation build generates a machine-readable RAG dataset at
`build/rag/rsyslog_rag_db.json`. This is produced by `../build_rag_db.py`
and contains ~12,000 structured chunks for AI retrieval pipelines.

**Regenerate with:** `make -C doc json-formatter`

See [chunking_and_embeddings.md](chunking_and_embeddings.md) for schema details.

## Current canonical terms

- **Log pipeline** (internally also called *message pipeline*).
- Use `getting_started/beginner_tutorials/` (no `learning_path/`).

_Last reviewed: 2025-12-23_
