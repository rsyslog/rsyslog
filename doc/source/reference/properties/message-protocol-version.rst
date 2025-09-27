.. _prop-message-protocol-version:
.. _properties.message.protocol-version:

protocol-version
================

.. index::
   single: properties; protocol-version
   single: protocol-version

.. summary-start

Carries the VERSION header field defined by RFC 5424 Section 6.2.2.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: protocol-version
:Category: Message Properties
:Type: string

Description
-----------
Contains the RFC 5424 Section 6.2.2 ``VERSION`` header field that signals
which syslog message format the sender used. RFC 5424 assigns ``1`` to
the standard defined in the specification and reserves future numbers
for later revisions. When rsyslog consumes data without a native syslog
header, this property reflects the configured or inferred version value
for that input, commonly ``1``.

Usage
-----
.. _properties.message.protocol-version-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%protocol-version%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
