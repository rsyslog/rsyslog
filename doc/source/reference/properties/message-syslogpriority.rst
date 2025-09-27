.. _prop-message-syslogpriority:
.. _properties.message.syslogpriority:
.. _properties.alias.syslogpriority:

syslogpriority
==============

.. index::
   single: properties; syslogpriority
   single: syslogpriority

.. summary-start

Provides the same numeric value as ``syslogseverity`` for historical reasons.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogpriority
:Category: Message Properties
:Type: integer
:Aliases: syslogseverity

Description
-----------
An alias for ``syslogseverity`` - included for historical reasons (be careful: it
still is the severity, not PRI!).

Usage
-----
.. _properties.message.syslogpriority-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%syslogpriority%")

Aliases
~~~~~~~
- syslogseverity — alias for syslogpriority

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
