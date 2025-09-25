.. _prop-system-time-year:
.. _properties.system-time.year:

$year
=====

.. index::
   single: properties; $year
   single: $year

.. summary-start

Provides the four-digit current year at processing time.

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

   template(name="show-year" type="string" string="%$year%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
