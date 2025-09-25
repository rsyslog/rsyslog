.. _prop-message-syslogseverity-text:
.. _properties.message.syslogseverity-text:

syslogseverity-text
===================

.. index::
   single: properties; syslogseverity-text
   single: syslogseverity-text

.. summary-start

Returns the severity from the message expressed as text.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogseverity-text
:Category: Message Properties
:Type: string
:Aliases: syslogpriority-text

Description
-----------
Severity from the message - in text form.

Usage
-----
.. _properties.message.syslogseverity-text-usage:

.. code-block:: rsyslog

   template(name="show-syslogseverity-text" type="string" string="%syslogseverity-text%")

Aliases
~~~~~~~
- syslogpriority-text — alias for syslogseverity-text

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
