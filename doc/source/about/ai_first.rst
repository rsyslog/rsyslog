.. _about-ai-first:
.. index:: AI First; Responsible AI; Golden Path; Guardrails; Safeguards

====================================================
AI-First (Human-Controlled): Principles and Practice
====================================================

.. meta::
   :description: rsyslog's Responsible AI First approach, original vision and rationale, plus Golden Path, Guardrails, Safeguards, and CI gates.
   :keywords: rsyslog, AI First, responsible AI, Golden Path, Guardrails, Safeguards, CI, documentation

Overview
========

Rsyslog's AI-First strategy means we embed AI as an enabling layer across
the whole lifecycle: idea, design, development, testing, documentation,
and user support. This is not limited to docs. It is a disciplined, long-term
approach that aims to deliver faster, clearer outcomes without compromising
the fundamentals rsyslog is known for.

A Personal Word from the Lead Developer
=======================================

Over decades of shifts in computing, we have learned to evaluate new waves
critically and pragmatically. We treat AI the same way: as a tool that can
speed up routine work and expand coverage, while humans remain responsible
for direction, decisions, and release quality. We systematically probe AI on
hard edge cases and use what we learn to refine prompts and improve code and
docs so they are easier to validate and maintain.

Why AI-First?
=============

- Users want focused answers fast. AI helps surface concise, context-relevant
  guidance instead of long pages to sift through.
- rsyslog contains decades of logging know-how, but writing and refreshing
  traditional docs is costly. AI multiplies our impact.
- AI will shape how software is developed and supported. We prefer to lead,
  not follow, and to show how to do this responsibly.

Our Vision
==========

We integrate AI into each stage:

- Idea & Design: assist in sketching proposals, comparing options, extracting
  patterns from feature requests.
- Development & Testing: aid code reading, propose tests, and stress obscure
  edge cases that are expensive to craft by hand.
- Documentation & Knowledge: restructure and clarify material for humans and
  machines, with consistent metadata that improves retrieval.
- User Support: offer fast, context-aware help via the rsyslog Assistant.

Addressing AI Criticism
=======================

- AI augments humans; it does not replace them. Core maintainers review and
  verify output.
- We value transparency. We disclose where AI assists and keep quality under
  human control.
- Quality remains first-class. AI takes on repetitive work so experts can focus
  on design, correctness, and hard problems.

Key Principles
==============

- Human-in-Control: experts never accept suggestions blindly.
- Continuous Improvement: adopt better tools as models evolve; keep standards.
- Openness: the approach is documented and visible to the community.

What Comes Next
===============

- Advancing AI-powered log analysis and observability tooling.
- Iterative improvements to docs, examples, and onboarding.
- Expanding the rsyslog Assistant and related support capabilities.

Looking Forward
===============

AI-First is a strategic investment, not a shortcut. We are connecting code,
docs, support, and AI into one lifecycle that provides fast, accurate, and
reliable answers. This complements the values rsyslog has always stood for:
reliability, performance, and careful engineering.

Core Framing
============

.. _ai-first-golden-path:
.. index:: Golden Path

Golden Path
-----------

The Golden Path is the recommended, low-friction way of working that leads
to high-quality results. We make the right thing the easy thing:

- Prefer RainerScript for new configuration examples.
- Provide quickstart environments (for example docker-compose with sane defaults).
- Use the rsyslog Commit Assistant to draft clear, policy-compliant messages.
- Publish structured docs with stable metadata sections to help humans and AIs.
- Use the rsyslog Assistant for fast self-help on configuration and troubleshooting.

.. _ai-first-guardrails:
.. index:: Guardrails

Guardrails
----------

Guardrails are proactive constraints that shape acceptable behavior:

- Coding and style rules (for example clang-format, header/comment policy).
- Documentation structure (parameter pages with metadata tables).
- Contribution templates (Issues/PRs with required sections).
- Prompt and process rules for AI usage (consult rsyslog docs first, cite
  sections, prefer latest stable, refuse when uncertain).
- Commit message standards (descriptive, small, reviewable diffs).
- Inline provenance notes in AI-assisted files, pointing to this page.
- AI reviewer bot provides advisory feedback on PRs.
- Metadata template required for parameter documentation pages.

Guardrails set expectations up-front. They do not have to block to be useful,
but pair with safeguards for enforcement.

.. _ai-first-safeguards:
.. index:: Safeguards

Safeguards
----------

Safeguards are enforcement and verification mechanisms that prevent harm and
catch errors deterministically:

- CI gates (multi-distro builds, tests, sanitizers, distcheck, coverage).
- Branch protection (required checks, review before merge).
- Static and security analysis (for example CodeQL, dependency updates).
- Release checklists and reproducible packaging steps.
- Selective CI execution (paths-ignore and targeted workflows).
- Buildbot extended checks (performance, long-running, packaging).

.. _ai-first-ci-safeguards:
.. index:: CI; GitHub Actions; Buildbot

Safeguards (CI gates we already run)
====================================

Our GitHub Actions and Buildbot infrastructure provide strong, deterministic
safeguards on every pull request. These automated checks ensure quality and
stability before code can be merged:

- Deterministic PR matrix across many distros/OS: CentOS 7/8, Debian 10/11,
  Ubuntu 18/20/22/24, Fedora 35/36; plus focused jobs (Kafka, Elasticsearch).
  Runs via ``.github/workflows/run_checks.yml``.
- Memory/thread sanitizers: ASan/UBSan (clang on Ubuntu 22), TSan (Ubuntu 24)
  with strict fail-on-error and tuned suppression files.
- ``distcheck`` gating (packaging-quality build/test) on Ubuntu 22, aborting
  on failures.
- Coverage jobs (gcov/codecov), including Kafka-focused coverage.
- Kafka integration: dedicated workflow ``.github/workflows/run_kafka_distcheck.yml``
  runs containerized builds/tests with strict flags.
- Systemd journal integration: dedicated ``.github/workflows/run_journal.yml`` workflow.
- Style and config hygiene: code style check and ``yamllint`` run per PR.
- Docs build: Sphinx documentation build runs on PRs to catch regressions.
- Static security analysis: CodeQL workflow active on the repo.
- Fast preflight: lightweight "PR Validation" job provides quick feedback
  before the heavy matrix runs.
- Selective execution: large matrix triggers only on code-relevant paths;
  heavy suites are split into targeted workflows.
- Additional coverage: Buildbot (private configuration) runs extended and
  costly tests (for example performance, long-running, packaging).

How to verify
-------------

- Browse recent workflow runs: GitHub -> Actions -> All workflows.
- Inspect workflow definitions in the repo:
  ``.github/workflows/run_checks.yml``, ``.github/workflows/run_kafka_distcheck.yml``, and
  ``.github/workflows/run_journal.yml``.

.. _ai-first-visible-artifacts:
.. index:: Visible Artifacts

Visible Artifacts
=================

These are user-facing elements that demonstrate the process and can be
verified by any contributor:

- rsyslog Assistant (AI help tool for configuration and troubleshooting).
- rsyslog Commit Assistant (commit message helper).
- AI reviewer bot summaries/reviews on PRs (advisory).
- Descriptive commits with clear rationale and structure (enforced via review).
- Inline provenance notes in AI-assisted files.
- Issue and PR templates guiding contributors to supply needed info.
- Metadata template applied to parameter documentation pages.
- RAG Knowledge Base (``build/rag/rsyslog_rag_db.json``): A machine-readable
  dataset of ~12,000 structured chunks extracted from all documentation pages,
  enabling AI-powered search and context injection for the rsyslog Assistant.
  Regenerated automatically by CI; build locally with ``make json-formatter``.

.. _ai-first-roles:
.. index:: Roles; Human-in-Control

Roles and Accountability
========================

- Humans own strategy, architecture, merge decisions, and releases.
- AI assists with drafting, refactoring proposals, examples, and summaries.
- Reviewers verify correctness, rationale, and alignment with project goals.
- Maintainers ensure guardrails and safeguards stay effective over time.

.. _ai-first-operational-rules:
.. index:: Operational Rules; Contribution Policy

Operational Rules (condensed)
=============================

- Cite authoritative rsyslog docs when AI has influenced content.
- Prefer stable references (versioned docs, labeled sections).
- Keep AI-generated code changes small and reviewable.
- Explain reasoning in PR descriptions; link to design/context notes when relevant.
- Accept that deterministic CI gates are non-negotiable.

.. _ai-first-changelog:
.. index:: Change Log

Change Log
==========

- 2025-08-19: Restored context from the original AI-First page and expanded with
  Golden Path, Guardrails, Safeguards, CI safeguards, visible artifacts, and
  verification pointers; added anchors and index entries.
- 2025-07-xx: Initial version aligned with "Clarifying AI First".

References
==========

- Clarifying AI First - what it really means for rsyslog
- Shipping better docs with AI - restructuring module parameters
- rsyslog Assistant (experimental AI help)
