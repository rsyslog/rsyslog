.. _prop-message-pri:
.. _properties.message.pri:

pri
===

.. index::
   single: properties; pri
   single: pri

.. summary-start

Provides the undecoded PRI header value defined by RFC 5424 Section 6.2.1.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: pri
:Category: Message Properties
:Type: string

Description
-----------
Contains the raw ``PRI`` header value from RFC 5424 Section 6.2.1, which is
written as ``<PRIVAL>`` in front of the syslog message. The integer inside
the angle brackets encodes facility and severity per ``PRIVAL = facility * 8
+ severity``. Facilities 0 through 23 and severities 0 through 7 follow the
tables in RFC 5424 (Tables 1 and 2). When rsyslog reads data without a
syslog header, the ``PRI`` value is derived from input metadata or
configuration so downstream templates can still compute facility and
severity.

Usage
-----
.. _properties.message.pri-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%pri%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
