.. _prop-message-msgid:
.. _properties.message.msgid:

msgid
=====

.. index::
   single: properties; msgid
   single: msgid

.. summary-start

Carries the MSGID field defined by the syslog protocol draft.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: msgid
:Category: Message Properties
:Type: string

Description
-----------
The contents of the MSGID field from IETF draft
``draft-ietf-syslog-protocol``.

Usage
-----
.. _properties.message.msgid-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%msgid%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
