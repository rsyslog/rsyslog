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
Timestamp captured when the operating system hands the message to rsyslog's
reception buffer. The value reflects the precise moment rsyslog accepted the
event, before any queueing or ruleset processing begins.

If several messages are delivered in the same receive buffer (for example when
TCP transports multiple entries together), they all carry the same
``timegenerated`` stamp because rsyslog accepted them at the same instant.

The timestamp remains stable even if the message spends time in an in-memory or
disk queue. As a result, ``timegenerated`` is usually newer than
``timereported`` (which comes from the sender) but still older than ``$now``,
which is evaluated later during processing.

Usage
-----
.. _properties.message.timegenerated-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="timegenerated")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
