.. _prop-system-bom:
.. _properties.system.bom:

$bom
====

.. index::
   single: properties; $bom
   single: $bom

.. summary-start

Provides the UTF-8 byte-order mark that can prefix Unicode output.

.. summary-end

This property belongs to the **System Properties** group.

:Name: $bom
:Category: System Properties
:Type: string

Description
-----------
The UTF-8 encoded Unicode byte-order mask (BOM). This may be useful in
templates for RFC5424 support, when the character set is known to be Unicode.

Usage
-----
.. _properties.system.bom-usage:

.. code-block:: rsyslog

   template(name="show-bom" type="string" string="%$bom%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
