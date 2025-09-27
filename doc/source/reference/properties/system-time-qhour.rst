.. _prop-system-time-qhour:
.. _properties.system-time.qhour:

$qhour
======

.. index::
   single: properties; $qhour
   single: $qhour

.. summary-start

Indicates the current quarter-hour within the hour.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $qhour
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current quarter hour we are in. Much like :ref:`prop-system-time-hhour`, but values range from 0 to 3
(for the four quarter hours that are in each hour).

Usage
-----
.. _properties.system-time.qhour-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="$qhour")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
