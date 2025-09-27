.. _prop-message-pri-text:
.. _properties.message.pri-text:

pri-text
========

.. index::
   single: properties; pri-text
   single: pri-text

.. summary-start

Outputs the PRI field in text form with the numeric PRI appended.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: pri-text
:Category: Message Properties
:Type: string

Description
-----------
The PRI part of the message in a textual form with the numerical PRI appended in
brackets (e.g. "local0.err<133>").

Usage
-----
.. _properties.message.pri-text-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%pri-text%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
