.. _prop-message-procid:
.. _properties.message.procid:

procid
======

.. index::
   single: properties; procid
   single: procid

.. summary-start

Carries the PROCID header field defined in RFC 5424 Section 6.2.6.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: procid
:Category: Message Properties
:Type: string

Description
-----------
Carries the ``PROCID`` header field defined in RFC 5424 Section 6.2.6. The
field has no fixed syntax; senders often use it for a process name, PID,
transaction identifier, or other origin-specific token, and RFC 5424
allows the NILVALUE (``-``) when no value applies. When rsyslog ingests
messages without a syslog header, this property holds the configured or
inferred value for the source instead of a transmitted header field.

Usage
-----
.. _properties.message.procid-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="procid")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
