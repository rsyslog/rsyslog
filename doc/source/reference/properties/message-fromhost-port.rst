.. _prop-message-fromhost-port:
.. _properties.message.fromhost-port:

fromhost-port
=============

.. index::
   single: properties; fromhost-port
   single: fromhost-port

.. summary-start

Reports the numeric source port of the sender when available.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: fromhost-port
:Category: Message Properties
:Type: string

Description
-----------
The same as ``fromhost``, but contains the numeric source port of the sender.
Local inputs provide an empty string.

Usage
-----
.. _properties.message.fromhost-port-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%fromhost-port%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
