# Chunking & Embeddings

Guidelines for documentation structure and the automated RAG extraction process.

## Authoring Guidelines

- Target 400–900 words per page.
- One diagram and one code block typical.
- Use lists; avoid giant paragraphs.
- Maintain logical independence per file.

## RAG Knowledge Base

The automated extraction is performed by [build_rag_db.py](../build_rag_db.py),
which processes Sphinx doctrees and generates `build/rag/rsyslog_rag_db.json`.

### Chunk Types

| Type | Description |
|------|-------------|
| `section_header` | Document section titles with hierarchy context |
| `text_block` | Prose content (~2000 chars, merged thematically) |
| `code` | Configuration examples and code blocks |

### Metadata Schema

Each chunk contains:

- **chunk_id**: Stable MD5 hash for deduplication
- **parent_id**: Links to parent section for context retrieval
- **attributes**:
  - `module`: rsyslog module name (e.g., imfile, omhttp) or "core"
  - `scope`: Parameter scope (input, action, module, general)
  - `item`: Specific parameter or section name
- **vector_text**: Optimized for embedding search
- **llm_text**: Full context with syntax templates for LLM consumption

### Syntax Templates

For parameter documentation, the extractor generates hints like:
```
input(type="imfile" file="...")
action(type="omhttp" server="...")
module(load="impstats" interval="...")
```

### Regeneration

```bash
# Full build with RAG extraction
make -C doc json-formatter

# Or use the alias
make -C doc rag-db
```

The CI workflow automatically builds and uploads the RAG artifact on every PR.
