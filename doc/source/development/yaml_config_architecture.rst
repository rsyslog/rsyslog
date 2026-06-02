.. _dev_yaml_config_architecture:

YAML Configuration Architecture
================================

.. meta::
   :description: Developer guide to rsyslog YAML config architecture: translation layer, nvlst intermediate representation, and RainerScript back-end sharing.
   :keywords: yaml, configuration, architecture, nvlst, rainerscript, translation layer, cnfobj, cnfDoObj, yamlconf

.. summary-start

YAML config is a thin translation front-end: yamlconf.c converts .yaml files into the same nvlst/cnfobj structures RainerScript produces, feeding them to the shared cnfDoObj() back-end.

.. summary-end


Overview
--------

rsyslog's YAML support is a first step toward a structured, operator-friendly
configuration format.  The guiding principle was to **minimise the change surface**:
rather than building a parallel engine, ``runtime/yamlconf.c`` translates each YAML
block into the same intermediate representation (``cnfobj`` + ``nvlst``) that the
lex/bison RainerScript grammar already produces.

This confinement strategy means:

- Parameter validation, type coercion, and error reporting reuse the existing
  ``nvlstGetParams()`` layer — no duplication.
- Every module that works in RainerScript works identically in YAML.
- Defects in the translation layer cannot silently corrupt the shared back-end.
- The YAML-specific code is a single, self-contained file with no runtime presence
  after configuration loading completes.

This approach may be revisited if future requirements (richer error messages,
schema validation, live reload) justify a deeper integration.  Any such refactoring
requires clear evidence of user benefit that outweighs the additional maintenance
surface.


Processing Pipeline
-------------------

.. mermaid::

   flowchart TD
     YF["YAML file<br>(.yaml / .yml)"]:::src
     LY["libyaml parser<br>(event stream)"]:::parse
     NV["nvlst chains<br>(name-value lists)"]:::ir
     CO["cnfobj<br>(typed config objects)"]:::ir
     CD["cnfDoObj()<br>(rsconf.c dispatcher)"]:::core
     MH["Module handlers<br>(global / input / action …)"]:::core
     SC["script: / statements:<br>content"]:::src
     CB["cnfAddConfigBuffer()<br>(RainerScript text)"]:::parse
     BP["lex / bison parser<br>(existing grammar)"]:::parse
     RE["Ruleset engine<br>(execution tree)"]:::core

     YF --> LY
     LY --> NV
     NV --> CO
     CO --> CD
     CD --> MH
     SC -->|"verbatim or<br>synthesised"| CB
     CB --> BP
     BP --> RE

     classDef src    fill:#fff2cc,stroke:#d6b656;
     classDef parse  fill:#dae8fc,stroke:#6c8ebf;
     classDef ir     fill:#f8cecc,stroke:#b85450;
     classDef core   fill:#d5e8d4,stroke:#82b366;

**Legend:** Yellow — input files/text. Blue — parsing stages.
Red — intermediate representation. Green — shared rsyslog core back-end.


Key Structures
--------------

**``nvlst``** (``runtime/conf.h``) is an ordered linked list of ``(name, value)``
pairs — rsyslog's canonical intermediate representation for configuration
parameters.  Both the RainerScript grammar and ``yamlconf.c`` produce ``nvlst``
chains; ``nvlstGetParams()`` is format-agnostic.

**``cnfobj``** wraps an ``nvlst`` with a type tag (``CNFOBJ_GLOBAL``,
``CNFOBJ_ACTION``, ``CNFOBJ_INPUT``, etc.).  One ``cnfobj`` is constructed per
top-level YAML block.

**``cnfDoObj()``** (``rsconf.c``) receives a ``cnfobj``, reads the type tag, and
calls the same module initialisation function that RainerScript would have called.


Ruleset Script Handling
-----------------------

YAML offers three ways to express ruleset logic; all three end up in the same
RainerScript execution tree via ``cnfAddConfigBuffer()``:

- **``script:``** — raw RainerScript block, passed through verbatim.
- **``statements:``** — YAML-native ``if / set / unset / call / foreach`` maps;
  ``yamlconf.c`` synthesises them into a RainerScript string.
- **``filter:`` + ``actions:``** — one-level shortcut; synthesised into an
  ``if … then { … }`` fragment.

No separate interpreter exists.  The lex/bison parser handles all ruleset logic.


Parity and Maintenance
----------------------

Because both formats share the back-end, parity is largely automatic:

- **New global parameters** — no YAML change needed; ``nvlstGetParams()`` picks
  them up automatically.
- **New top-level statement types** — requires a ``parse_*`` function in
  ``yamlconf.c`` and an entry in the dispatch table.
- **Renamed/removed parameters** — update ``yamlconf.c`` if it special-cases the
  name, and update the user docs for both formats.

See ``runtime/AGENTS.md``: *"Any change to config objects, statement types,
template modifiers, or global parameters must be reflected here as well as in
``grammar/``."*


See Also
--------

- :doc:`architecture` — rsyslog microkernel architecture overview
- :doc:`design_decisions` — libyaml library choice rationale
- :doc:`config_data_model` — rsyslog configuration object model
- :ref:`yaml_config` — user-facing YAML reference
