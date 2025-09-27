.. _prop-message-fromhost:
.. _properties.message.fromhost:

fromhost
========

.. index::
   single: properties; fromhost
   single: fromhost

.. summary-start

Reports the hostname of the system from which the message was received.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: fromhost
:Category: Message Properties
:Type: string

Description
-----------
Hostname of the system the message was received from (in a relay chain, this is
the system immediately in front of us and not necessarily the original sender).
This is a DNS-resolved name, except if that is not possible or DNS resolution
has been disabled. Reverse lookup results are cached; see
:ref:`reverse_dns_cache` for controlling cache timeout. Forward lookups for
outbound connections are not cached by rsyslog and are resolved via the system
resolver whenever a connection is made.

Usage
-----
.. _properties.message.fromhost-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%fromhost%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
