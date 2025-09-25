.. _prop-message-syslogpriority-text:
.. _properties.message.syslogpriority-text:
.. _properties.alias.syslogpriority-text:

syslogpriority-text
===================

.. index::
   single: properties; syslogpriority-text
   single: syslogpriority-text

.. summary-start

Provides the same text severity description as ``syslogseverity-text``.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogpriority-text
:Category: Message Properties
:Type: string
:Aliases: syslogseverity-text

Description
-----------
An alias for ``syslogseverity-text``.

Usage
-----
.. _properties.message.syslogpriority-text-usage:

.. code-block:: rsyslog

   template(name="show-syslogpriority-text" type="string" string="%syslogpriority-text%")

Aliases
~~~~~~~
- syslogseverity-text — canonical property referenced by syslogpriority-text

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
