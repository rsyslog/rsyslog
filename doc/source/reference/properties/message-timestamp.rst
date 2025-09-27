.. _prop-message-timestamp:
.. _properties.message.timestamp:

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

Description
-----------
Alias for ``timereported``.

Usage
-----
.. _properties.message.timestamp-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%timestamp%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
