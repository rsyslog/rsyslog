.. _prop-system-time-minute:
.. _properties.system-time.minute:

$minute
=======

.. index::
   single: properties; $minute
   single: $minute

.. summary-start

Returns the current minute as a two-digit number.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $minute
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current minute (2-digit).

Usage
-----
.. _properties.system-time.minute-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="$minute")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
