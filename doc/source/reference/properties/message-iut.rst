.. _prop-message-iut:
.. _properties.message.iut:

iut
===

.. index::
   single: properties; iut
   single: iut

.. summary-start

Holds the monitorware InfoUnitType value when present.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: iut
:Category: Message Properties
:Type: string

Description
-----------
The monitorware InfoUnitType - used when talking to a
`MonitorWare <https://www.monitorware.com>`_ backend (also for
`Adiscon LogAnalyzer <https://loganalyzer.adiscon.com/>`_).

Usage
-----
.. _properties.message.iut-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="iut")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
