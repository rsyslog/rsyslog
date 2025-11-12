.. _prop-system-time-year:
.. _properties.system-time.year:

$year
=====

.. index::
   single: properties; $year
   single: $year

.. summary-start

Returns the current year as a four-digit number.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $year
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current year (4-digit).

Usage
-----
.. _properties.system-time.year-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="$year")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
