.. _tut-04-message-pipeline:

The Message Pipeline: Inputs → Rulesets → Actions
#################################################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog pipeline, input, ruleset, action, kafka

.. summary-start

Understand rsyslog’s core architecture: messages enter through **inputs**, are processed by **rulesets**,
and end up in one or more **actions** (outputs).

.. summary-end

Goal
====

Build a correct mental model of rsyslog’s architecture using the **message pipeline**.
This will help you predict where to add filters, which actions will run, and why your snippets behave as they do.

The three core components
=========================

1. **Inputs** – how messages arrive.

   Examples: ``imuxsock`` (syslog socket), ``imjournal`` (systemd journal),
   ``imfile`` (text files).

2. **Rulesets** – the logic in between.

   They hold filters and actions, and decide what happens to each message.

3. **Actions** – where messages go.

   Examples: files (``omfile``), remote syslog (``omfwd``), or modern targets
   like Kafka (``omkafka``).

Visual model
============

.. mermaid::

   flowchart LR
     A["Inputs"] --> B["Ruleset"]
     B --> C1["Action 1"]
     C1 --> C2["Action 2"]

     subgraph S["Actions (serial by default)"]
       C1
       C2
     end

Explanation
===========

- **Inputs** feed messages into rsyslog.
- Messages enter a **ruleset**, which decides what to do.
- The **actions** are a *set of destinations* that run **in order by default** (serial).
  Concurrency is possible (e.g., per-action queues), but that’s advanced and not needed here.

Example: add a second destination for a tag
===========================================

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

   Forwarding requires a **second machine** (or another rsyslog instance
   listening on a port). Without a reachable target **and** without an action
   queue, rsyslog will retry and may appear “stuck” for a minute or longer
   before the next attempt. For a proper walkthrough, see :doc:`../forwarding_logs`.

Action order and config sequence
================================

- Actions execute **serially in the order they appear**.
- Earlier actions can **modify or discard** messages before later actions see them.
- Include snippets in ``/etc/rsyslog.d/`` are processed in **lexical order**
  (e.g., ``10-first.conf`` runs before ``50-extra.conf``).
- You can introduce concurrency by giving an action its **own queue** (``action.queue.*``)
  or by using separate rulesets/workers — advanced topics, not needed here.

See :doc:`05-order-matters` for a hands-on demo of ordering effects.

Verification checkpoint
=======================

You should now be able to:

- Sketch **Input → Ruleset → Actions** from memory.
- Recognize where your distro-provided **inputs** attach to the flow.
- Understand that **actions** are a grouped set of destinations that run **in order** by default.
- Name at least one modern output (e.g., **Kafka**).

See also / Next steps
=====================

At this point you have installed rsyslog, created your first config, and understood
the default setup. Next, we build on that foundation:

- :doc:`02-first-config` – your first action and targeted filtering.
- :doc:`03-default-config` – how distro inputs fit the pipeline.
- :doc:`../forwarding_logs` – basics of forwarding and queues.
- :doc:`05-order-matters` – how config and file order influence behavior.

----

.. tip::

   🎬 *Video idea (2–3 min):* show the diagram, then run
   ``logger -t tut04 "…"`` and watch the message hit both the file and the
   forwarder; point out that actions execute sequentially.
