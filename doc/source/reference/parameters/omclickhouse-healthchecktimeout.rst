.. _param-omclickhouse-healthchecktimeout:
.. _omclickhouse.parameter.input.healthchecktimeout:

healthCheckTimeout
==================

.. index::
   single: omclickhouse; healthCheckTimeout
   single: healthCheckTimeout
   single: omclickhouse; healthchecktimeout
   single: healthchecktimeout

.. summary-start

Sets the timeout, in milliseconds, for verifying ClickHouse availability.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: healthCheckTimeout
:Scope: input
:Type: int (milliseconds)
:Default: 3500
:Required?: no
:Introduced: not specified

Description
-----------
This parameter sets the timeout for checking the availability of ClickHouse. Value is given in milliseconds.

Input usage
-----------
.. _omclickhouse.parameter.input.healthchecktimeout-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" healthCheckTimeout="5000")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
