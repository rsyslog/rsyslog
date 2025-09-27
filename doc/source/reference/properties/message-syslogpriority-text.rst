.. _prop-message-syslogpriority-text:
.. _properties.message.syslogpriority-text:

syslogpriority-text
===================

.. index::
   single: properties; syslogpriority-text
   single: syslogpriority-text

.. summary-start

Returns the same textual severity string as :ref:`prop-message-syslogseverity-text`.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: syslogpriority-text
:Category: Message Properties
:Type: string

Description
-----------
An alias for :ref:`prop-message-syslogseverity-text`. The rendered string matches
the derived severity name even when no syslog header was present on receipt.

Usage
-----
.. _properties.message.syslogpriority-text-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="syslogpriority-text")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
