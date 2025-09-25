.. _prop-message-hostname:
.. _properties.message.hostname:

hostname
========

.. index::
   single: properties; hostname
   single: hostname

.. summary-start

Provides the hostname value carried inside the message header.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: hostname
:Category: Message Properties
:Type: string
:Aliases: source

Description
-----------
Hostname from the message.

Usage
-----
.. _properties.message.hostname-usage:

.. code-block:: rsyslog

   template(name="show-hostname" type="string" string="%hostname%")

Aliases
~~~~~~~
- source — alias for hostname

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
