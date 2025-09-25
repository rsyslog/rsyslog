.. _prop-message-msg:
.. _properties.message.msg:

msg
===

.. index::
   single: properties; msg
   single: msg

.. summary-start

Returns the MSG part of the syslog message.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: msg
:Category: Message Properties
:Type: string

Description
-----------
The MSG part of the message (aka "the message" ;)).

Usage
-----
.. _properties.message.msg-usage:

.. code-block:: rsyslog

   template(name="show-msg" type="string" string="%msg%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
