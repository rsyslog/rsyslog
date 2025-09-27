.. _prop-message-pri:
.. _properties.message.pri:

pri
===

.. index::
   single: properties; pri
   single: pri

.. summary-start

Provides the undecoded PRI part of the syslog message.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: pri
:Category: Message Properties
:Type: string

Description
-----------
PRI part of the message - undecoded (single value).

Usage
-----
.. _properties.message.pri-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%pri%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
