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
follow the CEE/lumberjack spec. The so-called "CEE cookie" is checked
and, if present, the JSON-encoded structured message content is parsed.
The properties are then available as original message properties.

As a convenience, mmjsonparse will produce a valid CEE/lumberjack log
message if passed a message without the CEE cookie.  A JSON structure
will be created and the "msg" field will be the only field and it will
contain the message. Note that in this case, mmjsonparse will
nonetheless return that the JSON parsing has failed.

The "CEE cookie" is the character sequence "@cee:" which must prepend the
actual JSON. Note that the JSON must be valid and MUST NOT be followed
by any non-JSON message. If either of these conditions is not true,
mmjsonparse will **not** parse the associated JSON. This is based on the
cookie definition used in CEE/project lumberjack and is meant to aid
against an erroneous detection of a message as being CEE where it is
not.

This also means that mmjsonparse currently is NOT a generic JSON parser
that picks up JSON from wherever it may occur in the message. This is
intentional, but future versions may support config parameters to relax
the format requirements.


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
   * - :ref:`param-mmjsonparse-cookie`
     - .. include:: ../../reference/parameters/mmjsonparse-cookie.rst
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

Apply default normalization
---------------------------

This activates the module and applies normalization to all messages.

.. code-block:: none

  module(load="mmjsonparse")
  action(type="mmjsonparse")


Permit parsing messages without cookie
--------------------------------------

To permit parsing messages without cookie, use this action statement

.. code-block:: none

  action(type="mmjsonparse" cookie="")


.. toctree::
   :hidden:

   ../../reference/parameters/mmjsonparse-cookie
   ../../reference/parameters/mmjsonparse-userawmsg
   ../../reference/parameters/mmjsonparse-container

