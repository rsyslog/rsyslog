.. _prop-message-msgid:
.. _properties.message.msgid:

msgid
=====

.. index::
   single: properties; msgid
   single: msgid

.. summary-start

Carries the MSGID header field defined in RFC 5424 Section 6.2.7.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: msgid
:Category: Message Properties
:Type: string

Description
-----------
Carries the ``MSGID`` header field defined in RFC 5424 Section 6.2.7. The
value identifies the semantic type of the event (for example an SMTP
transaction label) and RFC 5424 allows the NILVALUE (``-``) when the
sender cannot provide one. When rsyslog processes messages that lacked a
syslog header at ingestion, the property contains the configured or
inferred MSGID value rather than an on-the-wire header.

Usage
-----
.. _properties.message.msgid-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="msgid")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
