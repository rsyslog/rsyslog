.. _prop-message-fromhost-ip:
.. _properties.message.fromhost-ip:

fromhost-ip
===========

.. index::
   single: properties; fromhost-ip
   single: fromhost-ip

.. summary-start

Provides the message source as an IP address just like :ref:`prop-message-fromhost`.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: fromhost-ip
:Category: Message Properties
:Type: string

Description
-----------
The same as :ref:`prop-message-fromhost`, but always as an IP address. Local inputs (like
imklog) use 127.0.0.1 in this property.

Usage
-----
.. _properties.message.fromhost-ip-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="fromhost-ip")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
