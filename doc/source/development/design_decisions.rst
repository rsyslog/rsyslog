.. meta::
   :description: Design decisions and architectural choices in rsyslog development.
   :keywords: rsyslog, design, architecture, libyaml, decisions

.. _design_decisions:

Design Decisions
================

.. summary-start

Records significant architectural and library choices made during rsyslog development.

.. summary-end

This document records significant architectural and library choices made during the development of rsyslog, providing context for future contributors.

Library Choices
---------------

Libyaml
~~~~~~~
**Context:**
The introduction of complex configuration objects (e.g., `ratelimit()`) requiring external policy files necessitated a structured data format. YAML was chosen for its readability and widespread adoption.

**Decision:**
Use `libyaml` (https://github.com/yaml/libyaml) as the YAML parser.

**Reasoning:**
1.  **Portability:** `libyaml` is written in C89, ensuring compatibility with the wide range of legacy and enterprise platforms rsyslog supports (AIX, Solaris, older Linux distributions) without requiring a modern C++ compiler or complex toolchain.
2.  **Standardization:** It is the reference implementation for YAML in C. Widely used wrappers in other languages (Python, Ruby) rely on it, ensuring its behavior is the de-facto standard.
3.  **Dependencies:** It has no external dependencies, keeping rsyslog's dependency footprint minimal.
4.  **License:** The MIT license is permissive and fully compatible with rsyslog's licensing model.
5.  **Alternatives Considered:**
    
    *   `libfyaml`: While feature-rich and supporting YAML 1.2, it is newer and less proven on obscure architectures compared to `libyaml`.
    *   `libcyaml`: Adds a layer of convenience but introduces an additional dependency on top of `libyaml`.
