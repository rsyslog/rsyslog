.. _tut-04-log-pipeline:

The Log Pipeline: Inputs → Rulesets → Actions
#############################################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog log pipeline, message pipeline, input, ruleset, action, kafka, output

.. summary-start

Understand rsyslog’s core architecture: logs enter through **inputs**, are processed by **rulesets**, 
and end up in one or more **actions** (outputs). This flow is called the **log pipeline** — 
historically known as the *message pipeline*.

.. summary-end


Goal
====

Build a correct mental model of rsyslog’s architecture using the **log pipeline**.  
This helps you predict where to add filters, which actions will run, and why your
snippets behave as they do.

The three core components
=========================

1. **Inputs** – how logs arrive.  
   Examples: ``imuxsock`` (syslog socket), ``imjournal`` (systemd journal),
   ``imfile`` (text files).

2. **Rulesets** – the logic in between.  
   They hold filters and actions and decide what happens to each message.

3. **Actions** – where logs go.  
   Examples: files (``omfile``), remote syslog (``omfwd``), or modern targets
   like Kafka (``omkafka``).

Visual model
============

.. mermaid::

   flowchart LR
     A["Inputs"]:::input --> B["Ruleset"]:::ruleset
     B --> C1["Action 1"]:::action
     B --> C2["Action 2"]:::action

     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;

Explanation
===========

- **Inputs** feed messages into rsyslog.
- Each message enters a **ruleset**, which decides what to do with it.
- **Actions** are destinations that execute **in order by default** (serial).  
  Concurrency is possible through per-action queues or worker threads, 
  but that’s an advanced topic.

Example: add a second destination
=================================

Write messages tagged ``tut04`` to a file *and* forward them:

.. code-block:: rsyslog

   if ($programname == "tut04") then {
       action(type="omfile" file="/var/log/mypipeline.log")
       action(type="omfwd" target="logs.example.com" port="514" protocol="udp")
   }

Restart and test:

.. code-block:: bash

   sudo systemctl restart rsyslog
   logger -t tut04 "hello from rsyslog tutorial 04"
   sudo tail -n 20 /var/log/mypipeline.log

.. note::

   Forwarding requires a **second machine** or another rsyslog instance 
   listening on a port. Without a reachable target and without an action queue, 
   rsyslog will retry and may appear “stuck” for a short time before the next attempt.  
   For a proper walkthrough, see :doc:`../forwarding_logs`.

Action order and config sequence
================================

- Actions execute **serially in the order they appear**.
- Earlier actions can **modify or discard** messages before later ones run.
- Include snippets in ``/etc/rsyslog.d/`` are processed in **lexical order** 
  (e.g., ``10-first.conf`` runs before ``50-extra.conf``).
- Concurrency can be introduced by giving an action its **own queue** 
  (``action.queue.*``) or by using separate rulesets/workers.

See :doc:`05-order-matters` for a hands-on demo of ordering effects.

Verification checkpoint
=======================

You should now be able to:

- Sketch **Input → Ruleset → Actions** from memory.  
- Recognize where your distro-provided **inputs** attach to the flow.  
- Understand that **actions** are sequential by default.  
- Name at least one modern output (e.g., **Kafka**).  

See also / Next steps
=====================

You now understand the basic architecture — the **log pipeline**.  
To explore more advanced pipeline concepts (branching, staging, queues),
see:

- :doc:`../../concepts/log_pipeline/stages`
- :doc:`../../concepts/log_pipeline/design_patterns`
- :doc:`../forwarding_logs` - how forwarding and queues interact.
- :doc:`05-order-matters` - ordering and include file sequence.
- :doc:`../../concepts/log_pipeline/index` - conceptual overview

----

.. tip::

   🎬 *Video idea (2–3 min):* show the diagram, then run  
   ``logger -t tut04 "…"`` and watch the message hit both the file and the forwarder;  
   highlight that actions execute sequentially by default.
