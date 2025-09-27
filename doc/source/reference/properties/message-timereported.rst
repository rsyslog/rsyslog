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
Timestamp from the message. Resolution depends on what was provided in the
message (in most cases, only seconds).

Usage
-----
.. _properties.message.timereported-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%timereported%")

Aliases
~~~~~~~
- timestamp — alias for timereported

See also
--------
See :doc:`../../configuration/properties` for the category overview.
