.. _prop-message-iut:
.. _properties.message.iut:

iut
===

.. index::
   single: properties; iut
   single: iut

.. summary-start

Provides the MonitorWare InfoUnitType value from the message.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: iut
:Category: Message Properties
:Type: string

Description
-----------
The MonitorWare InfoUnitType - used when talking to a
`MonitorWare <https://www.monitorware.com>`_ backend (also for
`Adiscon LogAnalyzer <https://loganalyzer.adiscon.com/>`_).

Usage
-----
.. _properties.message.iut-usage:

.. code-block:: rsyslog

   template(name="show-iut" type="string" string="%iut%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
