.. _prop-system-time-month:
.. _properties.system-time.month:

$month
======

.. index::
   single: properties; $month
   single: $month

.. summary-start

Provides the current month as a two-digit number.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $month
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current month (2-digit).

Usage
-----
.. _properties.system-time.month-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="$month")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
