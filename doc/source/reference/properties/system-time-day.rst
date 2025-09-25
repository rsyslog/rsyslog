.. _prop-system-time-day:
.. _properties.system-time.day:

$day
====

.. index::
   single: properties; $day
   single: $day

.. summary-start

Provides the two-digit day of the month at processing time.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $day
:Category: Time-Related System Properties
:Type: string

Description
-----------
The current day of the month (2-digit).

Usage
-----
.. _properties.system-time.day-usage:

.. code-block:: rsyslog

   template(name="show-day" type="string" string="%$day%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
