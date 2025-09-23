.. _ref-mmaudit:

Linux Audit Log Parser (mmaudit)
================================

.. list-table::
   :widths: 25 75

   * - **Module Name:**
     - **mmaudit**
   * - **Author:**
     - `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
   * - **Available since:**
     - 7.x series (introduced during the rsyslog v7 development cycle)

Purpose
-------

The **mmaudit** message modification module detects Linux Audit records
and exposes their data as structured properties. When a message
matches the expected auditd format the module parses the record and adds
a JSON subtree under ``$!audit``. Subsequent actions can query those
fields when routing, filtering, or formatting audit events.

The plugin is optional at build time. Use
``./configure --enable-mmaudit`` to compile it when building rsyslog
from source.

Message detection and parsing
-----------------------------

``mmaudit`` operates on the raw message string. After trimming leading
whitespace the module expects the record to start with
``type=<digits> audit(``. Messages that do not match this pattern are
left untouched.

When the prefix is present ``mmaudit`` extracts two header elements:

* **type** – the numeric audit record type parsed from the digits that
  follow ``type=``.
* **auditid** – the identifier found between ``audit(`` and the closing
  ``):`` (typically ``timestamp:sequence``).

The remainder of the record is interpreted as a sequence of
whitespace-separated ``name=value`` pairs. Values may be unquoted (terminated
by a space) or wrapped in single or double quotes. The extracted data is inserted into a JSON
object called ``data`` beneath the ``$!audit`` subtree. Each value is
stored without surrounding quotes.

.. note::

   The parser does not currently skip the terminating quote character.
   As a result, the next field name begins with that quote (and possibly
   a space). Access such fields either by spelling the generated name
   literally (for example ``%$!audit!data!" exe%``) or by using the
   ``get_property()`` function to fetch the value by key.

JSON output structure
---------------------

After a successful parse the following properties become available:

* ``$!audit!hdr!type`` – integer containing the numeric audit type.
* ``$!audit!hdr!auditid`` – the audit identifier string.
* ``$!audit!data`` – JSON object with one entry per ``name=value`` pair
  in the record body.

The original message text is preserved. The module also sets the message
parse flag, so later actions can test ``if $parsesuccess == "on"`` to
check whether ``mmaudit`` produced structured data.

Configuration Parameters
------------------------

.. note::

   Parameter names are case-insensitive; camelCase is recommended for
   readability.

Module Parameters
~~~~~~~~~~~~~~~~~

This module has no module parameters.

Action Parameters
~~~~~~~~~~~~~~~~~

This module has no action parameters. Simply configure
``action(type="mmaudit")`` in the processing chain.

Usage example
-------------

The snippet below parses audit records, writes the structured payload to
an auxiliary file, and demonstrates how to access one of the generated
fields.

.. code-block:: none

   module(load="mmaudit")

   template(name="audit-json"
            type="string"
            string="%timegenerated% %HOSTNAME% %syslogtag% %$!audit%\n")

   action(type="mmaudit")
   if $parsesuccess == "on" then {
       # The field name contains the quote that preceded it in the log.
       set $.exe = get_property($!audit!data, '" exe');
       action(type="omfile"
              file="/var/log/audit-json.log"
              template="audit-json")
   }

Caveats
-------

* ``mmaudit`` performs no authenticity checks. Any log line that matches
  the expected syntax is treated as an audit record.
* Only records that begin with a numeric ``type=<digits>`` token followed
  by `` audit(`` are parsed. Records already translated to textual types
  (for example ``type=SYSCALL``) are ignored.
* Field names following quoted values include the trailing quote from
  that value, as described above.
