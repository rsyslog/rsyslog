.. _prop-system-time-hour:
.. _properties.system-time.hour:

$hour
=====

.. index::
   single: properties; $hour
   single: $hour

.. summary-start

Returns the current hour in 24-hour format.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $hour
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current hour in military (24 hour) time (2-digit).

Usage
-----
.. _properties.system-time.hour-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="$hour")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
