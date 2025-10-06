.. _log-pipeline-patterns:

========================
Pipeline Design Patterns
========================

.. meta::
   :description: Common rsyslog log pipeline design patterns, including fan-out, staged rulesets, and workload isolation.
   :keywords: rsyslog, log pipeline, design patterns, fan-out, ruleset chaining, queue isolation, configuration

.. summary-start

Explores practical log pipeline design patterns in rsyslog — how to branch data
to multiple destinations, chain rulesets for staged processing, and isolate
workloads with separate queues.

.. summary-end


Overview
--------
A **log pipeline** is flexible by design.  You can branch it, chain multiple
rulesets together, or separate workloads into isolated queues.  
These design patterns help adapt rsyslog to different environments — from
simple local logging to complex, multi-tenant ingestion systems.

Each pattern below includes a short diagram and explanation.

Fan-out (branching)
-------------------
The most common pattern: send each message to multiple destinations in parallel.
This provides redundancy or enables different consumers (archive, search, alerting).

.. mermaid::

   flowchart LR
     R["Ruleset"]:::ruleset --> A1["omfile<br>(local archive)"]:::action
     R --> A2["omrelp<br>(reliable ship)"]:::action
     R --> A3["omelasticsearch<br>(search index)"]:::action

     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;

**How it works:**  
Each action in a ruleset runs independently.  rsyslog queues each action’s output,
so one slow destination does not delay the others.

Example:

.. code-block:: rsyslog

   ruleset(name="MainFlow") {
     action(type="omfile" file="/var/log/app.log")
     action(type="omrelp" target="central.example.net" port="2514")
     action(type="omelasticsearch" server="es01" index="logs-app")
   }

**Use cases:**
- Store locally and forward reliably.
- Split between long-term archive and search analytics.
- Maintain an audit copy on local disk.

Staged processing (chaining)
----------------------------
Break complex logic into smaller, maintainable stages.
Use ``call`` to hand off between rulesets.

.. mermaid::

   flowchart LR
     I["Input(s)"]:::input --> R1["Ruleset<br>parse / normalize"]:::ruleset --> R2["Ruleset<br>enrich / filter"]:::ruleset --> R3["Ruleset<br>route / output"]:::ruleset --> A["Actions"]:::action

     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;

**How it works:**  
Each ruleset performs a specific task and then calls the next one:

.. code-block:: text

   ruleset(name="ParseStage") {
     action(type="mmjsonparse" container="$!data")
     call "EnrichStage"
   }

   ruleset(name="EnrichStage") {
     if $!data!level == "debug" then stop
     call "OutputStage"
   }

   ruleset(name="OutputStage") {
     action(type="omfile" file="/var/log/processed.json")
   }

**Benefits:**
- Easier to test and reason about individual stages.
- Enables modular reuse (different inputs can reuse the same enrich or output stage).
- Failures are isolated to one stage.

Workload isolation (multi-ruleset queues)
-----------------------------------------
Use different queues for different inputs or workloads to prevent one
source from overloading another.

.. mermaid::

   flowchart LR
     I1["Remote TCP"]:::input --> QR[("Queue<br>remote")]:::queue --> RR["Ruleset<br>remote"]:::ruleset --> AR["Remote<br>actions"]:::action
     I2["Local system"]:::input --> QL[("Queue<br>local")]:::queue --> RL["Ruleset<br>local"]:::ruleset --> AL["Local<br>actions"]:::action

     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;
     classDef queue fill:#f5f5f5,stroke:#999999,stroke-dasharray: 3 3;

**How it works:**  
Each input is assigned its own ruleset and queue.
This prevents bursty remote logs from starving local system messages.

Example:

.. code-block:: rsyslog

   module(load="imtcp")
   input(type="imtcp" port="514" ruleset="RemoteFlow")

   module(load="imuxsock")   # local system logs
   input(type="imuxsock" ruleset="LocalFlow")

   ruleset(name="RemoteFlow" queue.type="LinkedList" queue.size="10000") {
     action(type="omrelp" target="loghost" port="2514")
   }

   ruleset(name="LocalFlow" queue.type="LinkedList" queue.size="2000") {
     action(type="omfile" file="/var/log/local.log")
   }

**Best practices:**
- Choose queue sizes proportional to expected input volume.
- Use fewer worker threads for disk-bound outputs.
- Monitor queue backlog via ``impstats``.

Pattern selection guide
-----------------------

The table below summarizes which design pattern fits common scenarios.

+------------------------------------------+-------------------------+
| Scenario                                 | Recommended pattern     |
+==========================================+=========================+
| Multiple destinations, same data         | **Fan-out**             |
+------------------------------------------+-------------------------+
| Complex processing sequence              | **Staged rulesets**     |
+------------------------------------------+-------------------------+
| Independent workloads                    | **Workload isolation**  |
+------------------------------------------+-------------------------+

Conclusion
----------
Design patterns make rsyslog configurations scalable and maintainable.
Start simple with a single ruleset, then add branching or staging as your
log architecture grows.  Combine these patterns with tuned queues for
maximum throughput and resilience.

See also
--------
- :doc:`stages`
- :doc:`example_json_transform`
- :doc:`../queues`
- :doc:`../../configuration/modules/omfile`
- :doc:`../../configuration/modules/omrelp`
- :doc:`../../configuration/modules/mmjsonparse`
