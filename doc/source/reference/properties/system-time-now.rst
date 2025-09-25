.. _prop-system-time-now:
.. _properties.system-time.now:

$now
====

.. index::
   single: properties; $now
   single: $now

.. summary-start

Emits the current date stamp when the message is processed.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $now
:Category: Time-Related System Properties
:Type: string

Description
-----------
The current date stamp in the format YYYY-MM-DD.

Usage
-----
.. _properties.system-time.now-usage:

.. code-block:: rsyslog

   template(name="show-now" type="string" string="%$now%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
