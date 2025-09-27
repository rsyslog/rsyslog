.. _prop-message-protocol-version:
.. _properties.message.protocol-version:

protocol-version
================

.. index::
   single: properties; protocol-version
   single: protocol-version

.. summary-start

Carries the PROTOCOL-VERSION field defined by the syslog protocol draft.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: protocol-version
:Category: Message Properties
:Type: string

Description
-----------
The contents of the PROTOCOL-VERSION field from IETF draft
``draft-ietf-syslog-protocol``.

Usage
-----
.. _properties.message.protocol-version-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%protocol-version%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
