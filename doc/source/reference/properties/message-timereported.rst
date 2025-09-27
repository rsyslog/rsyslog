.. _prop-message-timereported:
.. _properties.message.timereported:
.. _properties.alias.timestamp:

timereported
============

.. index::
   single: properties; timereported
   single: timereported

.. summary-start

Captures the timestamp present in the original message header.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: timereported
:Category: Message Properties
:Type: timestamp
:Aliases: timestamp

Description
-----------
Timestamp copied from the sender supplied header. It represents when the
originating application believes the event occurred, so accuracy depends on the
remote clock, time zone configuration, and transport delays.

Older transports frequently provide only second-level resolution. Messages that
spent time on relays or queues can arrive with a ``timereported`` value that is
minutes or hours behind ``timegenerated`` or ``$now``. Treat the field as a
hint rather than an absolute truth when correlating events.

Usage
-----
.. _properties.message.timereported-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="timereported")
   }

Aliases
~~~~~~~
- timestamp — alias for timereported

See also
--------
See :doc:`../../configuration/properties` for the category overview.
