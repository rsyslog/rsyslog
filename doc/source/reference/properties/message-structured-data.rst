.. _prop-message-structured-data:
.. _properties.message.structured-data:

structured-data
===============

.. index::
   single: properties; structured-data
   single: structured-data

.. summary-start

Returns the STRUCTURED-DATA field from the syslog protocol draft.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: structured-data
:Category: Message Properties
:Type: string

Description
-----------
The contents of the STRUCTURED-DATA field from IETF draft
``draft-ietf-syslog-protocol``.

Usage
-----
.. _properties.message.structured-data-usage:

.. code-block:: rsyslog

   template(name="show-structured-data" type="string" string="%structured-data%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
