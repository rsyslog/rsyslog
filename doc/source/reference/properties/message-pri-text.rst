.. _prop-message-pri-text:
.. _properties.message.pri-text:

pri-text
========

.. index::
   single: properties; pri-text
   single: pri-text

.. summary-start

Formats the RFC 5424 PRI header as facility.severity with the numeric PRI appended.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: pri-text
:Category: Message Properties
:Type: string

Description
-----------
Formats the RFC 5424 Section 6.2.1 ``PRI`` value as ``facility.severity`` and
appends the numeric ``<PRIVAL>`` (for example ``local0.err<133>``). Facility
and severity names follow Tables 1 and 2 from RFC 5424. For inputs that lack a
syslog header, rsyslog derives the facility and severity from configuration or
other metadata before rendering this property.

Usage
-----
.. _properties.message.pri-text-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="pri-text")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
