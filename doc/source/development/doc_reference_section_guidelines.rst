# Documentation Reference Section Structure Guidelines

This page describes the desired layout for all _reference_ pages so they work well for humans (Furo theme) and for AI/RAG ingestion. It reflects the **current method**: every parameter lives in its own file in `doc/source/reference/parameters/`, and module pages show compact summary tables that include those files.

---

## Overview

- Keep each module page at `doc/source/configuration/modules/<module>.rst`.
- Move **all parameter details** into **per-parameter** files under `doc/source/reference/parameters/`.
- On the module page, replace long inline parameter blocks with two **list-tables** that:
  - list parameters,
  - include the short per-parameter summary from the parameter files.

No content loss. Preserve legacy names, defaults, version markers, warnings, and examples.

---

## Directory layout

```text
doc/
└── source/
    ├── configuration/
    │   └── modules/
    │       ├── imudp.rst
    │       ├── omfwd.rst
    │       └── omprog.rst                  # module pages remain here
    └── reference/
        └── parameters/
            ├── imudp-port.rst
            ├── imudp-rcvbufsize.rst
            ├── omfwd-target.rst
            ├── omfwd-protocol.rst
            └── omprog-command.rst          # one file per parameter
```

**Do not** create per-module subfolders for parameters. File names are flat and follow `<module>-<param-lower>.rst`.

---

## Per-parameter page template (RST)

Each parameter page must use this structure and anchors. Replace placeholders.

```rst
.. _param-<module>-<param-lower>:
.. _<module>.parameter.<scope>.<param-lower>:

<ParamName>
===========

.. index::
   single: <module>; <ParamName>
   single: <ParamName>

.. summary-start

<One concise present-tense sentence summarizing the parameter.>

.. summary-end

This parameter applies to :doc:`../../configuration/modules/<module>`.

:Name: <ParamName>
:Scope: <module|input>
:Type: <type>
:Default: <value>
:Required?: <yes|no>
:Introduced: <version text from original, or policy fallback>

Description
-----------

<Bring over original description verbatim or near-verbatim. Preserve
notes/warnings and versionadded/versionchanged blocks. Keep enumerations.>

<scope> usage
-------------

.. _param-<module>-<scope>-<param-lower>:
.. _<module>.parameter.<scope>.<param-lower>-usage:

.. code-block:: rsyslog

   <minimal but correct example using recommended casing>

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

<Only include if any exist. For each legacy token add a hidden anchor:>

.. _<module>.parameter.legacy.<legacy-canonical>:
- $<LegacyName> — maps to <ParamName> (status: legacy)

.. index::
   single: <module>; $<LegacyName>
   single: $<LegacyName>

See also
--------

See also :doc:`../../configuration/modules/<module>`.
```

### Casing policy for examples

- Parameter names are case-insensitive. In examples, **use camelCase** (recommended).
- Headings must keep the canonical parameter name as used in official docs.
- Do not use deprecated aliases in examples; keep them only in the Legacy section.

---

## Anchors and xref conventions

- **Canonical anchor**: `.. _param-<module>-<param-lower>:`
  Example: `param-imudp-port`
- **Scoped anchor**: `.. _<module>.parameter.<scope>.<param-lower>:`
  Scope is `module` or `input` only.
- **Usage anchors**: duplicate both above with `-usage` suffix placed next to the code block.
- Anchor leafs are **lowercase**, words separated by hyphens. Keep dots from names in headings, but normalize to hyphens in file and anchor leafs (e.g., `RateLimit.Interval` -> `ratelimit-interval`).
- From module pages, link parameters via `:ref:` to the **canonical** anchor, for example `:ref:`param-imudp-port``.

**No duplicate anchors** anywhere.

---

## Module page changes (`doc/source/configuration/modules/<module>.rst`)

Leave narrative content untouched (purpose, features, statistics, examples, performance notes, caveats). Replace inline parameter blocks with two list-tables.

1) Optional casing note placed right above parameters:

```rst
.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.
```

2) **Module Parameters** list-table:

```rst
Module Parameters
=================

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-<module>-<param-a>`
     - .. include:: ../../reference/parameters/<module>-<param-a>.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-<module>-<param-b>`
     - .. include:: ../../reference/parameters/<module>-<param-b>.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
```

3) **Input Parameters** list-table (same pattern; only include if the module has input-scoped params):

```rst
Input Parameters
================

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-<module>-<param-c>`
     - .. include:: ../../reference/parameters/<module>-<param-c>.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
```

4) Optionally append a **hidden toctree** listing all newly created parameter files for this module:

```rst
.. toctree::
   :hidden:

   ../../reference/parameters/<module>-<param-a>
   ../../reference/parameters/<module>-<param-b>
   ../../reference/parameters/<module>-<param-c>
```

---

## Naming rules

- Parameter files: `<module>-<param-lower>.rst`
- `<param-lower>` is the parameter name lowercased, with dots replaced by hyphens.
- Keep module names lowercased (e.g., `imudp`, `omfwd`).

---

## Content rules

- **No content loss**. Bring over all technical information, including defaults, types, units, enumerations, warnings, version markers, examples.
- Normalize legacy "binary" type to "boolean" in `:Type:`. If the term "binary" appears in original text, you may mention it in a note.
- Keep size units exactly as shown (e.g., `256k`, `1m`).
- Where a parameter was documented in multiple places, consolidate into the one parameter page and keep all content.

---

## Validation and quality gates

Sphinx build must be clean:

- No duplicate labels, no unknown references, no orphaned includes.
- All `:ref:` targets resolve.
- All `.. include::` paths exist.
- Every parameter from original Module/Input sections has a parameter file and a row in the correct list-table.
- Anchors use the correct scope:
  - Module params -> `.module.` anchors only.
  - Input params  -> `.input.` anchors only.
- Usage anchors end with `-usage` and are unique.
- Defaults, types, and introduced versions match original text.
- Legacy names appear only in the Legacy section of the parameter page, and only if they exist.

---

## Edge cases and fixes

- Fix mis-scoped anchors from the original (for example `.module.` vs `.input.`) and adjust internal references.
- Align inconsistent example casing to camelCase.
- Parameters with dots keep the dot in the **heading** but use hyphen in file and anchor leaf.
- If a parameter had both module and input variants, create two pages only if they are semantically different. Otherwise one page with the correct scope.

---

## Metadata and tags (optional but helpful)

At the top of each page you may add:

```rst
.. meta::
   :tag: module:<module>
   :tag: parameter:<param-lower>
```

Use tags consistently if you rely on them for AI/RAG filtering.

---

## TOC integration

If you maintain curated TOCs:

- In any developer documentation index that lists writing guides, include this page:

```rst
.. toctree::
   :maxdepth: 1

   reference_section_guidelines
```

- Module indices remain unchanged; module pages continue to live under `configuration/modules/`.

---

## Policy for "Introduced"

- Use the value from the original docs if present.
- If unknown, use the current project fallback policy for Introduced text.
- When a parameter lists any legacy equivalent directive, prefer the older fallback per policy.

---

## Checklist

- [ ] All parameters from the original Module/Input sections are covered.
- [ ] Sphinx builds with no duplicate or unknown labels.
- [ ] All includes and refs resolve.
- [ ] Anchors match conventions and scopes.
- [ ] Examples use camelCase; headings remain canonical.
- [ ] Legacy names appear only in the Legacy section and only if they exist.
- [ ] Narrative content outside parameter sections on the module page is unchanged.

