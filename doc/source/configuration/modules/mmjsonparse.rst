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

