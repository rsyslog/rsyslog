.. _prop-message-syslogfacility:
.. _properties.message.syslogfacility:

syslogfacility
==============

.. index::
   single: properties; syslogfacility
   single: syslogfacility

.. summary-start

Contains the numeric syslog facility extracted from the message.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogfacility
:Category: Message Properties
:Type: integer

Description
-----------
Reports the numeric syslog facility defined in RFC 3164 and RFC 5424 (Table 1 of
RFC 5424). The field is a 5-bit code describing the general origin of the
message (for example 3 for ``daemon`` or 20 for ``local4``). Facility values
combine with severity to form the PRI value (``PRI = facility * 8 + severity``),
but otherwise remain independent. For inputs received without a syslog header,
rsyslog supplies this value from configuration or other metadata.

Usage
-----
.. _properties.message.syslogfacility-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%syslogfacility%")

Notes
~~~~~
- Common facility codes: 0 ``kern``, 1 ``user``, 2 ``mail``, 3 ``daemon``,
  4 ``auth``, 5 ``syslog``, 6 ``lpr``, 7 ``news``, 8 ``uucp``, 9 ``cron``,
  10 ``authpriv``, 11 ``ftp``, 12-15 reserved/OS-specific, 16-23 ``local0``..
  ``local7``.
- Applications assign facilities themselves, so naming can diverge by
  platform. The ``local0``..``local7`` range is intentionally left for
  site-defined uses.
- Filter or route by facility, for example ``if $syslogfacility == 3`` or with
  legacy selectors such as ``mail.*``.

See also
--------
See :doc:`../../configuration/properties` for the category overview.
