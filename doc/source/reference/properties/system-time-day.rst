.. _prop-system-time-day:
.. _properties.system-time.day:

$day
====

.. index::
   single: properties; $day
   single: $day

.. summary-start

Returns the current day of the month as a two-digit number.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $day
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current day of the month (2-digit).

Usage
-----
.. _properties.system-time.day-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%$day%")

See also
--------
See :doc:`../../configuration/properties` for the category overview.
