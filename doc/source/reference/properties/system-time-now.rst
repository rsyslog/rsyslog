.. _prop-system-time-now:
.. _properties.system-time.now:

$now
====

.. index::
   single: properties; $now
   single: $now

.. summary-start

Yields the current local date stamp when the message is processed.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $now
:Category: Time-Related System Properties
:Type: timestamp

Description
-----------
Reports the local system date stamp (``YYYY-MM-DD``) at the moment rsyslog
processes the message. The value is independent of the event payload and
changes whenever the ruleset advances to the next message.

``$now`` is always a little newer than ``timegenerated`` because rsyslog only
evaluates it during processing, after the event has left the input queue. When
queues back up, the gap between the two can grow from fractions of a second to
minutes or hours.

Usage
-----
.. _properties.system-time.now-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="$now")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
