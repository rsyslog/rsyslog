.. _prop-message-syslogpriority:
.. _properties.message.syslogpriority:
.. _properties.alias.syslogpriority:

syslogpriority
==============

.. index::
   single: properties; syslogpriority
   single: syslogpriority

.. summary-start

Provides the same numeric severity value as ``syslogseverity`` for legacy use.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogpriority
:Category: Message Properties
:Type: integer
:Aliases: syslogseverity

Description
-----------
An alias for ``syslogseverity`` - included for historical reasons (be careful:
it still is the severity, not PRI!).

Usage
-----
.. _properties.message.syslogpriority-usage:

.. code-block:: rsyslog

   template(name="show-syslogpriority" type="string" string="%syslogpriority%")

Aliases
~~~~~~~
- syslogseverity — canonical property referenced by syslogpriority

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
