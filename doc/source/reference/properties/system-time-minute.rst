.. _prop-system-time-minute:
.. _properties.system-time.minute:

$minute
=======

.. index::
   single: properties; $minute
   single: $minute

.. summary-start

Provides the two-digit minute value at processing time.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $minute
:Category: Time-Related System Properties
:Type: string

Description
-----------
The current minute (2-digit).

Usage
-----
.. _properties.system-time.minute-usage:

.. code-block:: rsyslog

   template(name="show-minute" type="string" string="%$minute%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
