.. _prop-message-timestamp:
.. _properties.message.timestamp:

timestamp
=========

.. index::
   single: properties; timestamp
   single: timestamp

.. summary-start

Provides the same value as :ref:`prop-message-timereported` from the message header.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: timestamp
:Category: Message Properties
:Type: timestamp

Description
-----------
Alias for :ref:`prop-message-timereported`.

Usage
-----
.. _properties.message.timestamp-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="timestamp")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
