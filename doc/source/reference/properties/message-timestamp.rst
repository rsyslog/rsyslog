.. _prop-message-timestamp:
.. _properties.message.timestamp:
.. _properties.alias.timestamp:

timestamp
=========

.. index::
   single: properties; timestamp
   single: timestamp

.. summary-start

Provides the same timestamp as ``timereported`` for compatibility.

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

   template(name="show-timestamp" type="string" string="%timestamp%")

Aliases
~~~~~~~
- timereported — canonical property referenced by timestamp

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
