.. _prop-message-app-name:
.. _properties.message.app-name:

app-name
========

.. index::
   single: properties; app-name
   single: app-name

.. summary-start

Carries the APP-NAME header field defined in RFC 5424 Section 6.2.5.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: app-name
:Category: Message Properties
:Type: string

Description
-----------
Carries the ``APP-NAME`` header field defined in RFC 5424 Section 6.2.5,
which identifies the originator application with up to 48 printable
characters. RFC 5424 permits the NILVALUE (``-``) when an application
cannot supply a value. When rsyslog receives events without a native
syslog header (for example from imfile inputs or JSON
payloads), this property reflects a configured default or an inferred
placeholder instead of a transmitted header value.

Usage
-----
.. _properties.message.app-name-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%app-name%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
