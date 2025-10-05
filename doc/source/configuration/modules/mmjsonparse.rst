.. _ref-mmjsonparse:

*******************************************************
JSON Structured Content Extraction Module (mmjsonparse)
*******************************************************

:Module name: **mmjsonparse**
:Introduced: 6.6.0 (find-json mode added in 8.2510)
:Author: Rainer Gerhards <rgerhards@adiscon.com>

.. meta::
   :keywords: rsyslog, mmjsonparse, JSON, structured logging, parsing, find-json, cookie mode
   :description: Extracts JSON-structured fields from messages; supports legacy cookie-prefixed and find-json scanning modes. Exposes scan counters usable with Prometheus via imhttp.

.. summary-start

Parses JSON-structured content in log messages and exposes fields as message properties.
Supports legacy cookie-prefixed parsing and a flexible find-json scan mode.
Provides scan counters to assess success, failure, and truncation.

.. summary-end


Overview
========

``mmjsonparse`` extracts JSON content from incoming messages and exposes parsed
fields beneath the structured data container (for example, ``$!field``). It supports two
operating modes:

- **find-json (recommended):** scans the message for the first valid top-level JSON object and parses it.
- **cookie (legacy):** parses JSON only when the message starts with the legacy ``@cee:`` prefix.

Use ``mmjsonparse`` early in the processing pipeline to make JSON fields available for
filtering, normalization, or routing. The **cookie** mode is retained for backward
compatibility and should not be used in new deployments.

Introduced and Compatibility
----------------------------

The module has been available since rsyslog **6.6.0**.
The enhanced **find-json** scanning mode was introduced in version **8.2510** and is present in all later releases.

Behavior Notes
--------------

- This is not a generic validator. If messages contain trailing non-JSON after the parsed
  object and trailing content is not allowed, parsing is considered failed according
  to the configured policy.
- Parameter names are case-insensitive. For readability, camelCase is recommended.


Notable Features
================

- :ref:`mmjsonparse-parsing-result`
- :ref:`mmjsonparse-statistics`
- :ref:`mmjsonparse-failure-handling`
- :ref:`mmjsonparse-examples`


Parsing Modes
=============

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Mode
     - Description
   * - ``find-json``
     - Scans for the first top-level ``{...}`` anywhere in the message and parses it. Suited for modern app logs that embed JSON within other text.
   * - ``cookie`` (legacy)
     - Parses only when the message begins with ``@cee:`` and is immediately followed by valid JSON. Kept for backward compatibility with historic CEE/lumberjack emitters.


Configuration Parameters
========================

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmjsonparse-mode`
     - .. include:: ../../reference/parameters/mmjsonparse-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsonparse-cookie`
     - .. include:: ../../reference/parameters/mmjsonparse-cookie.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsonparse-max_scan_bytes`
     - .. include:: ../../reference/parameters/mmjsonparse-max_scan_bytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsonparse-allow_trailing`
     - .. include:: ../../reference/parameters/mmjsonparse-allow_trailing.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsonparse-userawmsg`
     - .. include:: ../../reference/parameters/mmjsonparse-userawmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsonparse-container`
     - .. include:: ../../reference/parameters/mmjsonparse-container.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


.. _mmjsonparse-parsing-result:

Check Parsing Result
====================

Use the ``$parsesuccess`` variable to check whether JSON parsing succeeded.

.. code-block:: rsyslog

   action(type="mmjsonparse" mode="find-json")

   if $parsesuccess == "OK" then {
       # downstream processing for structured messages
       action(type="omfile" file="/var/log/rsyslog/structured.log")
   } else if $parsesuccess == "FAIL" then {
       # handle failures (see dedicated section below)
       action(type="omfile" file="/var/log/rsyslog/nonconforming.log")
       stop
   }


.. _mmjsonparse-failure-handling:

Handling Parsing Failures
=========================

If JSON extraction fails, ``$parsesuccess`` is set to ``"FAIL"``. Best practice is to
keep such messages separate for inspection. Two common patterns are shown below.

A) Direct to non-conforming log with stop
-----------------------------------------

.. code-block:: rsyslog

   action(type="mmjsonparse" mode="find-json")

   if $parsesuccess == "FAIL" then {
       action(
         type="omfile"
         file="/var/log/rsyslog/nonconforming.log"
         template="RSYSLOG_TraditionalFileFormat"
       )
       stop    # prevent accidental downstream processing
   }

   # OK path continues as usual
   if $parsesuccess == "OK" then {
       action(type="omfile" file="/var/log/rsyslog/structured.log")
   }

B) Handoff to a dedicated inspection ruleset (with stop)
--------------------------------------------------------

.. code-block:: rsyslog

   ruleset(name="inspectNonConforming"){
       # capture raw for triage
       template(name="rawmsg" type="string" string="%rawmsg%\\n")
       action(type="omfile" file="/var/log/rsyslog/nonconforming.raw" template="rawmsg")
       # optional: richer snapshot for short-term debugging
       # action(type="omfile" file="/var/log/rsyslog/nonconforming.meta" template="RSYSLOG_DebugFormat")
       stop
   }

   action(type="mmjsonparse" mode="find-json")

   if $parsesuccess == "FAIL" then {
       call inspectNonConforming
   } else {
       action(type="omfile" file="/var/log/rsyslog/structured.log")
   }

Operational recommendations
---------------------------

- Use ``stop`` on the failure path if you must ensure the message does not reach later actions unintentionally.
  Omit ``stop`` if you intentionally want both paths.
- Apply a distinct retention policy to non-conforming logs; they can be bursty.
- For transient debugging, log ``%rawmsg%`` (and, if supported, an all-JSON snapshot) to aid triage.
- Run ``rsyslogd -N1`` after edits to validate configuration syntax.
- Consider preceding ``mmjsonparse`` with :doc:`mmutf8fix <mmutf8fix>` if invalid encodings are suspected.


.. _mmjsonparse-statistics:

Statistics Counters
===================

When using **find-json** mode, ``mmjsonparse`` maintains scan-related counters
(available via rsyslog’s statistics subsystem):

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Counter
     - Description
   * - ``scan.attempted``
     - Messages inspected by the find-json scanner.
   * - ``scan.found``
     - Messages where a JSON object was located (may still fail later on trailing rules).
   * - ``scan.failed``
     - Messages where a located JSON object could not be parsed (malformed).
   * - ``scan.truncated``
     - Scans aborted due to the ``max_scan_bytes`` limit.

Exposure and collection
-----------------------

- **impstats (recommended):** Configure ``impstats`` to emit periodic statistics as messages.
- **Prometheus via imhttp:** If :doc:`imhttp <imhttp>` is loaded with :ref:`metricsPath <imhttp-metricspath>`,
  it exposes rsyslog statistics (including ``mmjsonparse`` counters) in Prometheus text format at the configured HTTP path.
  See imhttp’s *Prometheus Metrics* section and secure endpoints as needed (for example, ``metricsBasicAuthFile``).

Performance guidance
--------------------

- **Success rate:** ``scan.found / scan.attempted`` — ability to locate JSON.
- **Parse quality:** ``scan.failed / scan.found`` — data quality problems.
- **Scan efficiency:** ``scan.truncated / scan.attempted`` — raise ``max_scan_bytes`` if frequent.


Processing Flow (informative)
=============================

.. note::
   The diagrams below are informational and do not alter behavior.

.. mermaid::
   :align: center

   flowchart TD
     A["msg in"] --> B["mmjsonparse<br>mode=find-json|cookie"]
     B -->| OK | C["fields under $!"]
     C --> D["structured path"]
     B -->| FAIL | E["non-conforming"]
     E --> F{"route"}
     F -->| file+stop | G["omfile<br>nonconforming.log"]
     F -->| ruleset+stop | H["inspectNonConforming()"]

Metrics exposure (optional)
---------------------------

.. mermaid::
   :align: center

   flowchart LR
     M["mmjsonparse counters"] --> S["stats registry"]
     S --> I["imhttp<br>/metrics"]
     I --> P["Prometheus scrape"]


.. _mmjsonparse-examples:

Examples
========

Enable default normalization (legacy cookie mode)
-------------------------------------------------

.. code-block:: rsyslog

   module(load="mmjsonparse")
   action(type="mmjsonparse" mode="cookie")


Permit parsing messages without cookie (do not require @cee:)
-------------------------------------------------------------

.. code-block:: rsyslog

   action(type="mmjsonparse" cookie="")


Find-JSON mode for embedded JSON content
----------------------------------------

.. code-block:: rsyslog

   # Basic find-json mode
   action(type="mmjsonparse" mode="find-json")

   # With limits and strict trailing control
   action(type="mmjsonparse"
          mode="find-json"
          max_scan_bytes="32768"
          allow_trailing="off")


Mixed mode processing
---------------------

.. code-block:: rsyslog

   if $msg startswith "@cee:" then {
       action(type="mmjsonparse" mode="cookie")
   } else if $msg contains "{" then {
       action(type="mmjsonparse" mode="find-json")
   }


See also
========

- :doc:`mmutf8fix <mmutf8fix>` — fix invalid UTF-8 before parsing
- :doc:`mmnormalize <mmnormalize>` — normalization after extraction
- :doc:`omelasticsearch <omelasticsearch>` — indexing structured logs
- :doc:`imhttp <imhttp>` — expose rsyslog statistics in Prometheus text format via HTTP


.. toctree::
   :hidden:

   ../../reference/parameters/mmjsonparse-mode
   ../../reference/parameters/mmjsonparse-cookie
   ../../reference/parameters/mmjsonparse-max_scan_bytes
   ../../reference/parameters/mmjsonparse-allow_trailing
   ../../reference/parameters/mmjsonparse-userawmsg
   ../../reference/parameters/mmjsonparse-container

