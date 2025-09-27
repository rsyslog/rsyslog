.. _prop-message-timegenerated:
.. _properties.message.timegenerated:

timegenerated
=============

.. index::
   single: properties; timegenerated
   single: timegenerated

.. summary-start

Records when rsyslog received the message with high resolution.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: timegenerated
:Category: Message Properties
:Type: timestamp

Description
-----------
Timestamp when the message was RECEIVED. Always in high resolution.

Usage
-----
.. _properties.message.timegenerated-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%timegenerated%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
