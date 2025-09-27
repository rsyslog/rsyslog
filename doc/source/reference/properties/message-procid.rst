.. _prop-message-procid:
.. _properties.message.procid:

procid
======

.. index::
   single: properties; procid
   single: procid

.. summary-start

Provides the PROCID field defined by the syslog protocol draft.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: procid
:Category: Message Properties
:Type: string

Description
-----------
The contents of the PROCID field from IETF draft
``draft-ietf-syslog-protocol``.

Usage
-----
.. _properties.message.procid-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%procid%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
