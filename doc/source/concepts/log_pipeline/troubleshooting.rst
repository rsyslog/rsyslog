.. _log-pipeline-troubleshooting:

========================
Pipeline Troubleshooting
========================

.. meta::
   :description: Troubleshooting steps and diagnostics for rsyslog log pipelines.
   :keywords: rsyslog, log pipeline, troubleshooting, queues, parsing, mmjsonparse, mmjsontransform, configuration, debug

.. summary-start

Guidelines and common checks for diagnosing problems in rsyslog log pipelines.
Covers syntax validation, module loading, parse errors, and queue behavior.

.. summary-end


Overview
--------
Even well-structured log pipelines can misbehave — messages may stop flowing,
queues can fill up, or transforms might silently fail.  
This page explains a quick, repeatable process to identify and fix issues
in rsyslog configurations that use the log pipeline model.

.. mermaid::

   flowchart LR
     I["Input"]:::input --> R["Ruleset"]:::ruleset --> A["Action"]:::action
     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;

   %% visually shows that problems can occur at any stage

Typical symptoms
----------------
- Messages received but not written to output files.
- High CPU usage or large backlog in queues.
- JSON transformation results missing or empty.
- Duplicate or reordered messages in outputs.
- Unexpected stop in forwarding (e.g., network blockages).

Basic validation steps
----------------------
1. **Check configuration syntax**
   - Run ``rsyslogd -N1`` before restarting to confirm there are no syntax errors.
   - Verify that all required modules load (look for ``module '...' loaded`` in startup logs).

2. **Inspect message content**
   - Add a temporary debug action to see the processed fields:

     .. code-block:: rsyslog

        action(type="omfile" file="/tmp/debug" template="RSYSLOG_DebugFormat")

   - Compare property values (e.g., ``$msg``, ``$!structured``) with expectations.

3. **Verify parsing success**
   - When using ``mmjsonparse`` or similar modules, check the variable ``$parsesuccess``.
   - Example guard condition:

     .. code-block:: rsyslog

        if $parsesuccess != "OK" then action(type="omfile" file="/var/log/parsefail.log")

4. **Monitor queues**
   - Use ``impstats`` to emit queue statistics and verify flow between stages.
   - Look for large ``size`` or ``enqueued`` values — these indicate bottlenecks.

Common causes and fixes
-----------------------
+------------------------------------+-----------------------------------------------+
| Problem                            | Typical solution                              |
+====================================+===============================================+
| Module not loaded                  | Add ``module(load="...")`` line and restart   |
+------------------------------------+-----------------------------------------------+
| JSON parsing fails                 | Use ``useRawMsg="on"`` or verify payload      |
+------------------------------------+-----------------------------------------------+
| Output file empty                  | Check ``$parsesuccess`` or ruleset logic      |
+------------------------------------+-----------------------------------------------+
| Queue never drains                 | Tune ``queue.workerThreads`` / ``queue.size`` |
+------------------------------------+-----------------------------------------------+
| Duplicate logs                     | Ensure only one ruleset receives input        |
+------------------------------------+-----------------------------------------------+
| Remote forwarding stopped          | Verify network reachability or RELP retry     |
+------------------------------------+-----------------------------------------------+

Advanced diagnostics
--------------------
- **Increase debug level**  
  Start rsyslog manually with ``rsyslogd -dn`` to trace the processing flow.
- **Use impstats for performance tuning**  
  Enable periodic statistics to monitor queue backlog, action latencies, and thread counts.
- **Isolate a stage**  
  Comment out actions or parsers to test where the flow breaks.
- **Check permissions**  
  Ensure rsyslog has write access to output directories.

Best practices
---------------
- Validate changes with ``rsyslogd -N1`` before deploying to production.
- Keep ``impstats`` enabled in complex deployments.
- Always gate expensive transforms (e.g., JSON parsing) with success checks.
- Use short, dedicated queues for slow destinations.

Conclusion
----------
Troubleshooting a log pipeline means following the data path — from input,
through ruleset logic, to output actions.  By validating configuration, verifying
queues, and monitoring parser status, you can quickly pinpoint where logs stop
and restore normal flow.

See also
--------
- :doc:`stages`
- :doc:`design_patterns`
- :doc:`example_json_transform`
- :doc:`../queues`
- :doc:`../../configuration/modules/impstats`
- :doc:`../../configuration/modules/mmjsonparse`
- :doc:`../../configuration/modules/mmjsontransform`
