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
The facility from the message - in numerical form.

Usage
-----
.. _properties.message.syslogfacility-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%syslogfacility%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
