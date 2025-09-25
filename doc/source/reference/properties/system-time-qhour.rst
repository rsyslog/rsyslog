.. _prop-system-time-qhour:
.. _properties.system-time.qhour:

$qhour
======

.. index::
   single: properties; $qhour
   single: $qhour

.. summary-start

Indicates which quarter hour within the current hour is active.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $qhour
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current quarter hour we are in. Much like ``$HHOUR``, but values range from
0 to 3 (for the four quarter hours that are in each hour).

Usage
-----
.. _properties.system-time.qhour-usage:

.. code-block:: rsyslog

   template(name="show-qhour" type="string" string="%$qhour%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
