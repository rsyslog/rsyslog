.. _prop-message-timereceived:
.. _properties.message.timereceived:

timereceived
============

.. index::
   single: properties; timereceived
   single: timereceived

.. summary-start

Alias for ``timegenerated`` that records when rsyslog received the message.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: timereceived
:Category: Message Properties
:Type: timestamp

Description
-----------
``timereceived`` is an alias for :ref:`prop-message-timegenerated`. It returns
the same timestamp: the point in time when the operating system hands the
message to rsyslog's reception buffer, before queueing or ruleset processing.

The alias exists because the older ``timegenerated`` name can be confused with
the sender-side timestamp. The sender-side timestamp remains
:ref:`prop-message-timereported`.

Usage
-----
.. _properties.message.timereceived-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="timereceived")
   }

See also
--------
See :doc:`message-timegenerated` for the detailed behavior and
:doc:`../../configuration/properties` for the category overview.
