.. _prop-message-structured-data:
.. _properties.message.structured-data:

structured-data
===============

.. index::
   single: properties; structured-data
   single: structured-data

.. summary-start

Provides the STRUCTURED-DATA field defined in RFC 5424 Section 6.3.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: structured-data
:Category: Message Properties
:Type: string

Description
-----------
Contains the ``STRUCTURED-DATA`` portion of a syslog message as defined in
RFC 5424 Section 6.3. The field holds zero or more SD-ELEMENT blocks with
ASCII SD-IDs and UTF-8 parameter values. When no structured data is
present, RFC 5424 requires the NILVALUE (``-``). For events ingested
without a syslog header, rsyslog populates this property with structured
data derived from configuration or input metadata, if any.

Usage
-----
.. _properties.message.structured-data-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="structured-data")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
