.. _prop-system-time-month:
.. _properties.system-time.month:

$month
======

.. index::
   single: properties; $month
   single: $month

.. summary-start

Provides the two-digit month of the year at processing time.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $month
:Category: Time-Related System Properties
:Type: string

Description
-----------
The current month (2-digit).

Usage
-----
.. _properties.system-time.month-usage:

.. code-block:: rsyslog

   template(name="show-month" type="string" string="%$month%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
