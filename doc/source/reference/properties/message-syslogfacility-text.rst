.. _prop-message-syslogfacility-text:
.. _properties.message.syslogfacility-text:

syslogfacility-text
===================

.. index::
   single: properties; syslogfacility-text
   single: syslogfacility-text

.. summary-start

Returns the textual syslog facility defined in RFC 5424 Table 1.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogfacility-text
:Category: Message Properties
:Type: string

Description
-----------
Returns the human-readable name of the syslog facility (such as ``daemon`` or
``local4``). The names follow the RFC 3164 and RFC 5424 mappings and correspond
directly to the numeric property :ref:`prop-message-syslogfacility`. When the
input does not supply a syslog header, rsyslog emits the configured or inferred
facility name for the message instead.

Usage
-----
.. _properties.message.syslogfacility-text-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%syslogfacility-text%")

Notes
~~~~~
- Expect the canonical names ``kern``, ``user``, ``mail``, ``daemon``,
  ``auth``, ``syslog``, ``lpr``, ``news``, ``uucp``, ``cron``, ``authpriv``,
  ``ftp``, system-reserved slots, and ``local0``..``local7``.
- Some systems expose small spelling differences; compare strings carefully if
  you expect portable configurations.
- Facility selection is independent from severity, but both values combine into
  ``PRI`` (``PRI = facility * 8 + severity``).

See also
--------
See :doc:`../../configuration/properties` for the category overview.
