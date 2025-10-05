.. _ref-mmjsonparse:

***********************************************************
JSON/CEE Structured Content Extraction Module (mmjsonparse)
***********************************************************

===========================  ===========================================================================
**Module Name:**             **mmjsonparse**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         6.6.0
===========================  ===========================================================================


Purpose
=======

This module provides support for parsing structured log messages that
follow the CEE/lumberjack spec or contain embedded JSON content.

In the legacy **cookie mode** (default), the module checks for the CEE cookie
and, if present, parses the JSON-encoded structured message content.
The properties are then available as original message properties.

In the **find-json mode**, the module scans the message content to locate
the first valid top-level JSON object "{...}" and parses it, regardless
of its position in the message. This mode is useful for processing logs
that contain JSON embedded within other text, such as application logs
with prefixes.

As a convenience, mmjsonparse will produce a valid CEE/lumberjack log
message if passed a message without the CEE cookie or valid JSON.  A JSON structure
will be created and the "msg" field will be the only field and it will
contain the message. Note that in this case, mmjsonparse will
nonetheless return that the JSON parsing has failed.

In **cookie mode**, the "CEE cookie" is the character sequence "@cee:" which must prepend the
actual JSON. Note that the JSON must be valid and MUST NOT be followed
by any non-JSON message. If either of these conditions is not true,
mmjsonparse will **not** parse the associated JSON. This is based on the
cookie definition used in CEE/project lumberjack and is meant to aid
against an erroneous detection of a message as being CEE where it is
not.

**Note:** Cookie mode is NOT a generic JSON parser that picks up JSON from
wherever it may occur in the message. This is intentional, but the new
find-json mode provides this capability.


Notable Features
================

- :ref:`mmjsonparse-parsing-result`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive. For readability, camelCase is
   recommended.


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

Check parsing result
====================

You can check whether rsyslogd was able to successfully parse the
message by reading the $parsesuccess variable :

.. code-block:: none

   action(type="mmjsonparse")
   if $parsesuccess == "OK" then {
      action(type="omfile" File="/tmp/output")
   }
   else if $parsesuccess == "FAIL" then {
      action(type="omfile" File="/tmp/parsing_failure")
   }


Statistics Counters
===================

When using find-json mode, mmjsonparse provides detailed statistics about JSON scanning performance and results. These counters are available through rsyslog's statistics interface and can be viewed using ``rsyslogctl stats`` or through the impstats module.

Available Counters
------------------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Counter Name
     - Description
   * - ``scan.attempted``
     - Total number of messages processed in find-json mode. Incremented for every message that enters find-json scanning, regardless of whether JSON is found.
   * - ``scan.found``
     - Number of messages where valid JSON objects were successfully located during scanning. This includes cases where JSON was found but later rejected due to trailing data restrictions.
   * - ``scan.failed``
     - Number of messages where JSON objects were found during scanning but failed to parse properly. This indicates malformed JSON content.
   * - ``scan.truncated``
     - Number of messages where the JSON scan was terminated due to reaching the ``max_scan_bytes`` limit before finding a complete JSON object.

Counter Usage Examples
----------------------

View current statistics:

.. code-block:: bash

   # View all rsyslog statistics including mmjsonparse
   rsyslogctl stats

   # Example output:
   {
     "name": "mmjsonparse",
     "origin": "mmjsonparse",
     "scan.attempted": 1523,
     "scan.found": 1456,
     "scan.failed": 42,
     "scan.truncated": 25
   }

Configure automatic statistics reporting:

.. code-block:: none

   # Report statistics every 60 seconds
   module(load="impstats" interval="60" severity="6")

   # Statistics will appear in rsyslog's main log

Performance Analysis
--------------------

The statistics counters help analyze JSON processing performance:

- **Success Rate**: ``scan.found / scan.attempted`` indicates how often JSON is successfully located
- **Parse Quality**: ``scan.failed / scan.found`` shows the rate of malformed JSON
- **Scan Efficiency**: ``scan.truncated / scan.attempted`` indicates if ``max_scan_bytes`` should be increased

Example analysis:

.. code-block:: none

   # High truncation rate suggests increasing max_scan_bytes
   if scan.truncated > (scan.attempted * 0.1) then {
       # Consider increasing max_scan_bytes parameter
   }

   # High failure rate suggests data quality issues
   if scan.failed > (scan.found * 0.05) then {
       # Investigate JSON formatting in source logs
   }

**Note**: Statistics counters are only active when using ``mode="find-json"``. Cookie mode does not generate these scanning-specific statistics.


Examples
========

Apply default normalization (legacy cookie mode)
------------------------------------------------

This activates the module and applies normalization to all messages using
the legacy CEE cookie mode.

.. code-block:: none

   module(load="mmjsonparse")
   action(type="mmjsonparse")


Permit parsing messages without cookie
--------------------------------------

To permit parsing messages without cookie, use this action statement

.. code-block:: none

   action(type="mmjsonparse" cookie="")


Find-JSON mode for embedded JSON content
-----------------------------------------

To parse JSON content embedded anywhere in the message, use find-json mode:

.. code-block:: none

   # Basic find-json mode with defaults
   action(type="mmjsonparse" mode="find-json")

   # Find-json mode with custom limits and strict trailing control
   action(type="mmjsonparse"
          mode="find-json"
          max_scan_bytes="32768"
          allow_trailing="off")


Mixed mode processing
---------------------

Different message types can be processed with different modes:

.. code-block:: none

   # Legacy CEE messages
   if $msg startswith "@cee:" then {
       action(type="mmjsonparse" mode="cookie")
   }

   # Modern logs with embedded JSON
   else if $msg contains "{" then {
       action(type="mmjsonparse" mode="find-json")
   }


.. toctree::
   :hidden:

   ../../reference/parameters/mmjsonparse-mode
   ../../reference/parameters/mmjsonparse-cookie
   ../../reference/parameters/mmjsonparse-max_scan_bytes
   ../../reference/parameters/mmjsonparse-allow_trailing
   ../../reference/parameters/mmjsonparse-userawmsg
   ../../reference/parameters/mmjsonparse-container

