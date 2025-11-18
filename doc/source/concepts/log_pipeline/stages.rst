.. _log-pipeline-stages:

====================
Pipeline Stages
====================

.. meta::
   :description: Detailed explanation of rsyslog log pipeline stages — inputs, rulesets, actions, and queues.
   :keywords: rsyslog, log pipeline, inputs, rulesets, actions, queues, architecture, reliability, concurrency

.. summary-start

Explains each stage of the rsyslog log pipeline — inputs, rulesets, actions, and queues —
and shows how they connect in a typical message flow.

.. summary-end


Overview
--------
In rsyslog, each event passes through a sequence of **pipeline stages** that define
how it is received, processed, and delivered.  Thinking in stages helps you design
configurations that are reliable, predictable, and easy to extend.

The four canonical stages are:

1. **Input** – receives data.
2. **Ruleset** – performs filtering and transformation.
3. **Action** – delivers processed results.
4. **Queue** – connects and buffers these stages.

The diagram below shows the basic flow.

.. mermaid::

   flowchart LR
     I["Input"]:::input --> Q1[("Input queue")]:::queue --> R["Ruleset"]:::ruleset --> Q2[("Action queue")]:::queue --> A["Action(s)"]:::action

     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;
     classDef queue fill:#f5f5f5,stroke:#999999,stroke-dasharray: 3 3;


Inputs
------
Inputs define **how rsyslog receives data** from the operating system or network.
They are modules whose names start with ``im`` (input module), such as:

- ``imuxsock`` – local UNIX socket for system logging.
- ``imjournal`` – systemd journal integration.
- ``imudp`` / ``imtcp`` – classic network syslog listeners.
- ``imfile`` – file tailing for arbitrary text sources.

Each input can be assigned a dedicated **ruleset**, giving it its own queue and
processing logic. This isolation allows different workloads (for example, remote
vs. local logs) to operate independently.

Example – assigning a ruleset to a TCP listener:

.. code-block:: rsyslog

   module(load="imtcp")
   input(type="imtcp" port="514" ruleset="RemoteLogs")

Rulesets
--------
Rulesets form the **logic and transformation stage** of the pipeline.
They consist of filters, conditions, and actions that process the properties
attached to each message.

Typical operations inside a ruleset:

- Evaluate filters with ``if/then`` or property-based comparisons.
- Parse or normalize data using modules such as ``mmjsonparse`` or ``mmnormalize``.
- Apply transformations, for example with ``mmjsontransform`` or ``mmfields``.
- Forward or store results using actions.

A ruleset is defined with the ``ruleset()`` directive and can be invoked from
another ruleset using ``call`` for staged processing.

Example:

.. code-block:: rsyslog

   ruleset(name="RemoteLogs") {
     if $fromhost-ip == "10.1.1.1" then stop
     action(type="mmjsonparse" container="$!structured")
     action(type="omfile" file="/var/log/remote.json")
   }

Actions
-------
Actions specify **what happens after processing**. They define the final destination
of the log message. Each action is executed sequentially within the ruleset’s
context, unless special control statements are used.

Common action types:

- ``omfile`` – write to file or named pipe.
- ``omfwd`` / ``omrelp`` – send to remote hosts.
- ``omelasticsearch`` / ``ommongodb`` – deliver to data stores.
- ``omstdout`` – useful for container logging.

Actions may have their own queues (``action.resumeRetryCount``, ``queue.size``, etc.)
to decouple delivery from processing speed.

Queues
------
Queues are the **connective tissue** of the pipeline. They buffer messages between
stages and manage concurrency.  Every major component — main, ruleset, or action —
can have an associated queue.

Queue types:

- ``Direct`` – minimal buffering, synchronous hand-off.
- ``LinkedList`` – in-memory queue, optionally disk-assisted.
- ``FixedArray`` – preallocated, low-latency but fixed-size.

Advantages of using queues:

- **Resilience:** disk spooling prevents message loss.
- **Rate control:** slow actions do not block inputs.
- **Parallelism:** multiple worker threads improve throughput.

Best practices
---------------
- Tune queue parameters (``queue.size``, ``queue.workerThreads``) for your workload.
- Use per-ruleset queues for isolation in multi-tenant setups.
- Monitor queue metrics with the ``impstats`` module.
- Prefer ``LinkedList`` queues unless very high message rates demand fine tuning.

Conclusion
----------
Each stage of the log pipeline — input, ruleset, action, and queue — is a modular
piece of rsyslog’s architecture.  Understanding their roles makes it easier to
build predictable, maintainable logging systems and to troubleshoot performance
issues when they arise.

See also
--------
- :doc:`../index`
- :doc:`design_patterns`
- :doc:`example_json_transform`
- :doc:`../../configuration/modules/imtcp`
- :doc:`../../configuration/modules/mmjsonparse`
- :doc:`../../configuration/modules/omfile`


.. _concept-model-concepts-log_pipeline-stages:

Conceptual model
----------------

- The pipeline is a staged flow: inputs ingest, rulesets transform, actions emit, and queues buffer between each boundary.
- Inputs can bind to specific rulesets, selecting both processing logic and queue isolation per ingress path.
- Rulesets encapsulate deterministic filtering and transformation; ``call`` composes them into multi-stage flows.
- Actions execute within a ruleset context, with optional per-action queues to absorb destination latency.
- Queue types govern durability and throughput: direct for synchronous handoff, memory for speed, disk-assisted for reliability.
- Worker threads and queue sizing shape concurrency and backpressure, preventing slow consumers from halting ingestion.
- Impstats telemetry and queue tuning are essential to maintain predictable latency under varying workloads.
