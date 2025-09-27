.. _prop-message-syslogseverity:
.. _properties.message.syslogseverity:
.. _properties.alias.syslogpriority:

syslogseverity
==============

.. index::
   single: properties; syslogseverity
   single: syslogseverity

.. summary-start

Provides the numeric syslog severity extracted from the message.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogseverity
:Category: Message Properties
:Type: integer
:Aliases: syslogpriority

Description
-----------
Reports the numeric syslog severity defined in RFC 3164 and RFC 5424 (Table 2 of
RFC 5424). The field is a 3-bit integer from 0 (``emerg``) to 7 (``debug``);
smaller numbers indicate higher urgency. The sending application chooses this
value, so real-world usage can vary and mappings are sometimes fuzzy. Severity
combines with the facility code to form the PRI value (``PRI = facility * 8 +
severity``). When rsyslog accepts messages without a syslog header, the
severity comes from configured defaults or input metadata rather than a value
that was transmitted on the wire.

Usage
-----
.. _properties.message.syslogseverity-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="syslogseverity")
   }

Notes
~~~~~
- Canonical severities:

  - **0** ``emerg``
  - **1** ``alert``
  - **2** ``crit``
  - **3** ``err``
  - **4** ``warning``
  - **5** ``notice``
  - **6** ``info``
  - **7** ``debug``
- Filter by number or name; for example ``if $syslogseverity <= 3 then ...``
  routes urgent messages (``emerg`` through ``err``).
- The textual property :ref:`prop-message-syslogseverity-text` exposes the same
  information as words. Legacy selector syntax such as ``*.err`` works as well.

Aliases
~~~~~~~
- syslogpriority — alias for syslogseverity

See also
--------
See :doc:`../../configuration/properties` for the category overview.
