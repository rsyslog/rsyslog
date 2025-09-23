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

   Parameter names are case-insensitive.


Action Parameters
-----------------

cookie
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "@cee:", "no", "none"

Permits to set the cookie that must be present in front of the
JSON part of the message.

Most importantly, this can be set to the empty string ("") in order
to not require any cookie. In this case, leading spaces are permitted
in front of the JSON. No non-whitespace characters are permitted
after the JSON. If such is required, mmnormalize must be used.


useRawMsg
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Specifies if the raw message should be used for normalization (on)
or just the MSG part of the message (off).


container
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "$!", "no", "none"

Specifies the JSON container (path) under which parsed elements should be
placed. By default, all parsed properties are merged into root of
message properties. You can place them under a subtree, instead. You
can place them in local variables, also, by setting path="$.".


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

