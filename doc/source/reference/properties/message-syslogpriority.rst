.. _prop-message-syslogpriority:
.. _properties.message.syslogpriority:

syslogpriority
==============

.. index::
   single: properties; syslogpriority
   single: syslogpriority

.. summary-start

Provides the same numeric value as :ref:`prop-message-syslogseverity` for historical reasons.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogpriority
:Category: Message Properties
:Type: integer

Description
-----------
An alias for :ref:`prop-message-syslogseverity` - included for historical reasons (be careful: it
still is the severity, not PRI!). The alias mirrors the same derived value even
for inputs without a syslog header.

Usage
-----
.. _properties.message.syslogpriority-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="syslogpriority")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
