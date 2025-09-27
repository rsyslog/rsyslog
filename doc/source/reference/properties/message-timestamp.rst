.. _prop-message-timestamp:
.. _properties.message.timestamp:
.. _properties.alias.timereported:

timestamp
=========

.. index::
   single: properties; timestamp
   single: timestamp

.. summary-start

Provides the same value as ``timereported`` from the message header.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: timestamp
:Category: Message Properties
:Type: timestamp
:Aliases: timereported

Description
-----------
Alias for ``timereported``.

Usage
-----
.. _properties.message.timestamp-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%timestamp%")

Aliases
~~~~~~~
- timereported — alias for timestamp

See also
--------
See :doc:`../../configuration/properties` for the category overview.
