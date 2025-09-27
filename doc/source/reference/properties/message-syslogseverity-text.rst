.. _prop-message-syslogseverity-text:
.. _properties.message.syslogseverity-text:
.. _properties.alias.syslogpriority-text:

syslogseverity-text
===================

.. index::
   single: properties; syslogseverity-text
   single: syslogseverity-text

.. summary-start

Returns the textual syslog severity defined in RFC 5424 Table 2.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogseverity-text
:Category: Message Properties
:Type: string
:Aliases: syslogpriority-text

Description
-----------
Returns the textual name of the syslog severity defined in RFC 3164 and RFC 5424
(Table 2, ``emerg`` through ``debug``). These strings describe how urgent the
sending application considered the event. They map directly to the numeric
values reported by :ref:`prop-message-syslogseverity`. When rsyslog processes a
message that lacked a syslog header, this property reports the configured or
inferred severity label instead of a name supplied by the sender.

Usage
-----
.. _properties.message.syslogseverity-text-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%syslogseverity-text%")

Notes
~~~~~
- Canonical severities: ``emerg``, ``alert``, ``crit``, ``err``, ``warning``,
  ``notice``, ``info``, ``debug``.
- Filter by name with comparisons such as ``if $syslogseverity-text == 'warning'``.
  Keep in mind that emitters may use small spelling variations like ``warn``.
- Severity remains independent from the facility; both combine into ``PRI``
  (``PRI = facility * 8 + severity``).

Aliases
~~~~~~~
- syslogpriority-text — alias for syslogseverity-text

See also
--------
See :doc:`../../configuration/properties` for the category overview.
