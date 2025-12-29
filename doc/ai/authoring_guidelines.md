# Authoring Guidelines (RST house style)

## Required header blocks
Anchors, meta, summary slices are mandatory.

```
.. _example-id:

.. meta::
   :description: <≤160 chars>
   :keywords: term1, term2

.. summary-start

Short one-paragraph summary (≤200 chars).

.. summary-end
```

### Applies to all `doc/source` pages
- Use these blocks on every new or materially edited RST page.
- If an existing page lacks the blocks, add them while editing.
- Module docs may add the blocks immediately after the module title and
  before the module metadata table/field list.

### Module documentation header
Module pages should include:
- `.. index:: ! <module>`
- Module metadata (module name, author/maintainer, introduced/version)
- A short summary slice per the required header blocks

Use `doc/ai/templates/template-module.rst` when creating or normalizing
module documentation.

## Tone
- Concepts: neutral, precise.
- Tutorials: friendly, runnable.
- Avoid buzzwords; explain plainly.

## Size & chunking (human + AI)
- Keep sections to 150–350 words; split longer sections with subheads.
- Prefer 1–3 short paragraphs per section and use lists for dense material.
- Keep pages to ~400–800 words unless a multi-page cluster is more appropriate.
