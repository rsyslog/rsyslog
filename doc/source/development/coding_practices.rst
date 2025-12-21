.. _coding-practices:

.. meta::
   :description: Home for rsyslog good coding practices, including patterns and antipatterns for the core and sibling libraries.
   :keywords: rsyslog, coding practices, patterns, antipatterns

.. summary-start

Defines where to document rsyslog coding practices and how to structure pattern and antipattern entries for the core and close sibling libraries such as liblognorm.

.. summary-end

rsyslog coding practices reference
==================================

Purpose and scope
-----------------

This page is the canonical home for **rsyslog good coding practices**. Use it to
record patterns and antipatterns that help humans and AI reason about the code
base. Its scope covers:

- The rsyslog core and built-in modules.
- Closely related libraries that share design DNA (for example, ``liblognorm``).
- Links to external per-library practice documents when they exist.

Placement and navigation
------------------------

- Keep the practice reference in this ``doc/source/development/`` page so it
  appears alongside other developer guides and is picked up by the development
  index.
- Add a short ``.. toctree::`` entry here for any deeper, topic-specific pages
  you create so navigation remains discoverable from the development section.
- Create one ``.rst`` file per practice for RAG-friendly ingestion and wire
  those files into the local ``.. toctree::`` so agents can fetch individual
  anchors without loading unrelated guidance.
- If a sibling library needs extra detail, add a short subsection here that
  links to the library-specific document in its own repository.
- When introducing a new practice, add anchor-friendly subsection titles so
  other docs and AI tools can deep-link to the guidance.
- Mention this page from ``AGENTS.md`` and other AI prompts so automated
  helpers pull the correct patterns and antipatterns into their context.

Referencing from sibling libraries
----------------------------------

When a closely related library such as ``liblognorm`` needs to reuse this
guidance:

- Point to this page as the canonical source from the sibling project's docs,
  release notes, or development guide (for example, a ``coding-practices``
  entry in its ``development`` or ``contributing`` index).
- Add a brief subsection in this page for the library with anchors that match
  the name used in its own docs so deep links stay stable.
- If the sibling project maintains extra practices, mirror the entry format and
  provide a link back here so readers can pivot between shared and
  library-specific rules.
- Keep cross-repo links stable by using the published rsyslog docs URL or the
  raw file link when referencing this page from other repositories.

Entry format
------------

Capture each practice (pattern or antipattern) in a consistent shape to make it
machine- and human-friendly:

- **Title and anchor**: ``.. _practice-name:`` plus a short, action-oriented
  heading.
- **Classification**: ``Pattern`` or ``Antipattern``.
- **Context**: The subsystem (for example, queueing, parsers, outputs, test
  harness) and any relevant modules.
- **Why it matters**: Risks avoided or benefits gained when following the
  practice.
- **Steps**: Checklist-style guidance that can be turned into linting or code
  review prompts.
- **References**: Pointers to implementation examples, tests, or design docs.

Organizing topics
-----------------

Group practices by the areas reviewers and tools check most often:

- **Concurrency and state**: Worker isolation, per-action locks, and avoiding
  WID leakage.
- **Memory and ownership**: Allocation conventions, lifetime rules, and cleanup
  expectations around action data and modules.
- **Error handling and logging**: Uniform error paths, return-code handling, and
  structured logging expectations.
- **Configuration and templates**: RainerScript guidance, template reuse, and
  validation hooks.
- **External dependencies**: How to guard optional libraries such as
  ``liblognorm`` and how to document minimum supported versions.

Contribution workflow
---------------------

Use the steps below when adding or updating a practice entry:

1. Choose the best section above or create a new subsection with a clear anchor.
2. Add a ``Pattern`` or ``Antipattern`` label and fill in the context, why, and
   steps bullets so they can be enforced by reviews and automated checks.
3. Link to concrete examples (source files, tests, or commit IDs) to ground the
   guidance.
4. Mention related guidance in sibling libraries (for example, link to a
   ``liblognorm`` rule) so maintainers can keep the documents in sync.
5. When practices change behavior, note migration tips for existing modules and
   call out any CI or linting hooks that should be updated.

.. _practice-catalog:

Practice catalog
----------------

Individual patterns and antipatterns live in their own files for easier
retrieval by humans and RAG pipelines. They are collected here via a short
toctree so navigation remains discoverable from the development guide.

.. toctree::
   :maxdepth: 1

   coding_practices/externalized_helper_contracts
   coding_practices/embedded_ipv4_tail_helper
   coding_practices/error_logging_with_errno
   coding_practices/defensive_coding

AI agent integration
--------------------

AI assistants (including prompt-driven doc builders or code generators) should
consume this page when preparing change plans or reviews:

- Add the ``coding_practices`` page to any retrieval or “context seeding” step
  that feeds AI agents, alongside module metadata and per-directory
  ``AGENTS.md`` guidance.
- Prefer deep links to specific practice anchors when citing expectations in
  generated plans, RAG snippets, or review comments.
- When patterns are updated, refresh downstream prompts (for example, commit
  helpers or doc-builder base prompts) so they keep injecting the latest
  practices.

Discoverability and expected pickup
-----------------------------------

The page is wired into the default AI workflow so it is easy to pull into
context:

- The repository-level ``AGENTS.md`` points to ``doc/AGENTS.md`` as the first
  stop for documentation work, and that guide explicitly calls out
  ``coding_practices.rst`` as mandatory RAG seed material.
- The page lives in the ``development`` toctree so Sphinx navigation and search
  surface it from the rendered docs even without AGENTS links.
- The doc-builder base prompt (``ai/rsyslog_code_doc_builder/base_prompt.txt``)
  tells agents to load this page before writing or revising guidance.
- Given those hooks, an agent that follows the documented prompt chain has a
  high likelihood (roughly 85–95%) of loading and honoring this guidance; risk
  primarily comes from prompts that ignore the AGENTS chain or operate on a
  partial checkout without the ``doc/`` tree.

Adaptation for sibling libraries
--------------------------------

For libraries like ``liblognorm``, keep a minimal subsection here with
library-specific anchors and point to the canonical document in that repository.
Mirror the entry format so AI tools can learn a single schema while respecting
per-library details.
