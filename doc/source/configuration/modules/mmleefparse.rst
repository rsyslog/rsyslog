.. _ref-mmleefparse:

LEEF Structured Content Extraction Module (mmleefparse)
=======================================================

**Module Name:**            **mmleefparse**

Purpose
=======

``mmleefparse`` parses log messages that follow the IBM Log Event Extended
Format (LEEF).  The module recognises the LEEF header (``LEEF:<version>|``)
followed by vendor, product, firmware version and event identifiers.  It then
parses the key/value segment of the event, honouring the delimiter used by the
sending device.  Parsed information is written back into the message as JSON so
that it can be processed with normal rsyslog property access.

The parser is implemented inside rsyslog and does not require a third-party
library.  No dedicated LEEF parsing library is currently known for the rsyslog
code base.

Parsing Result
==============

The module stores the extracted data under the configured container (default
``$!``).  Two JSON objects are created:

* ``header`` – contains the LEEF metadata: protocol version, vendor, product,
  product version, and event identifier.
* ``fields`` – contains all key/value pairs from the extension section.  Empty
  values are preserved as empty strings.  Elements such as descriptions or
  severities are represented as regular fields (for example ``EventName=``
  or ``sev=``) when present in the payload.

For example, parsing the sample Palo Alto Networks log with
``container="!leef"`` yields properties such as ``$!leef!header!vendor`` and
``$!leef!fields!src``.

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

   "string", "LEEF:", "no", "none"

Specifies the prefix that must appear in front of the LEEF payload.  Set the
cookie to an empty string when the message payload starts with the header
immediately.

useRawMsg
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Determines whether the raw message (``on``) or the parsed MSG part (``off``)
should be processed.  Earlier releases defaulted to ``off``; the module now
uses the raw message by default so that characters removed by rsyslog's MSG
normalisation remain available to the parser.

container
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "$!", "no", "none"

Defines the container into which the JSON result is written.  Set it to a
subtree (e.g. ``!leef``) if you want to keep the extracted values separated
from other structured data.

delimiter
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "\t", "no", "none"

Specifies the character that separates key/value pairs inside the extension
section.  LEEF 1.0 devices commonly use the tab character, but some send
``|`` or other characters.  ``mmleefparse`` accepts any single-character
delimiter and understands the escape sequences ``\t`` (tab), ``\n`` (newline)
and ``\\`` (backslash).

searchCookie
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Controls whether the parser scans for the cookie within the beginning of the
message.  When enabled (default) ``mmleefparse`` searches the first portion of
the payload to accommodate transport headers such as plain syslog that precede
the LEEF content.  Disable this option when you require an exact match at the
start of the message to minimise the risk of false positives.

searchWindow
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "64", "no", "none"

Defines how many characters are inspected when ``searchCookie`` is ``on``.
The window is scanned only from the front of the message so the cost remains
minimal even on high-volume deployments.  Increase the value if your transport
adds larger prefixes before the LEEF payload; reduce it or set
``searchCookie="off"`` for strict matching.

Examples
========

Parse standard tab-delimited events
-----------------------------------

.. code-block:: none

   module(load="mmleefparse")
   action(type="mmleefparse" container="!leef")
   if $parsesuccess == "OK" then {
       action(type="omfile" file="/var/log/leef-events.log")
   }

Parse events that use ``|`` as key/value delimiter
--------------------------------------------------

.. code-block:: none

   module(load="mmleefparse")
   action(type="mmleefparse" container="!leef" delimiter="|")
   if $parsesuccess == "OK" then {
       action(type="omfile" file="/var/log/leef-pan.log"
              template="outfmt")
   }

   template(name="outfmt" type="string"
            string="%$!leef!fields!src% -> %$!leef!fields!dst% %$!leef!fields!action%\n")

Sample message
--------------

The Palo Alto Networks sample below is parsed successfully with the
configuration from the previous example:

.. code-block:: none

   <14>Sep 17 13:45:35 firewall.domain.local \
   LEEF:1.0|Palo Alto Networks|PAN-OS Syslog Integration|11.1.6-h14|allow| \
   cat=TRAFFIC|ReceiveTime=2025/09/17 13:45:34|SerialNumber=010108010025| ...

(The ellipsis stands for the remainder of the key/value pairs.)
