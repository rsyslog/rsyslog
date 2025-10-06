.. _concept-log-pipeline:
.. _log.pipeline:

================
The Log Pipeline
================

.. meta::
   :description: Overview of the rsyslog log pipeline connecting inputs, rulesets, and actions.
   :keywords: rsyslog, log pipeline, message pipeline, input, ruleset, action, queue, architecture

.. summary-start

Introduces the rsyslog **log pipeline** — the flow of events from input to output through
rulesets and queues. This overview shows how logs move through rsyslog’s architecture.

.. summary-end


rsyslog processes logs through a **log pipeline** — internally called the *message pipeline*.
Each log message moves through three conceptual stages:

1. **Input:** collects data from sources (sockets, files, journal).
2. **Ruleset:** filters, parses, or transforms the message.
3. **Action:** outputs the processed log to its destination.

.. mermaid::

   flowchart LR
     subgraph "Input stage"
       I1["imkafka"]:::input
       I2["imjournal"]:::input
       I3["imfile"]:::input
       I4["..."]:::input
       I5["imtcp / imudp"]:::input
     end
     subgraph "Ruleset (logic)"
       F1["Filters<br>(if/then)"]:::ruleset
       P1["mmjsonparse"]:::ruleset
       T1["mmjsontransform"]:::ruleset
     end
     subgraph "Actions (outputs)"
       A1["omkafka"]:::action
       A2["omfwd / omrelp"]:::action
       A3["omhttp"]:::action
       A4["..."]:::action
       A5["omelasticsearch"]:::action
     end
     I1 --> F1
     I2 --> F1
     I3 --> F1
     I4 --> F1
     I5 --> F1
     F1 --> A1
     F1 --> A2
     F1 --> A3
     F1 --> A4
     F1 --> A5

     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;


Why this matters
----------------
Understanding the log pipeline helps you reason about reliability, performance,
and transformations. Every input, rule, and action is a building block that you can
compose into advanced pipelines with branching, staging, and queuing.

Subpages
--------

.. toctree::
   :maxdepth: 1
   :titlesonly:

   stages
   design_patterns
   example_json_transform
   troubleshooting
