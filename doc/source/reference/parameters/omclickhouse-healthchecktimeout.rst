.. _param-omclickhouse-healthchecktimeout:
.. _omclickhouse.parameter.module.healthchecktimeout:

healthCheckTimeout
==================

.. index::
   single: omclickhouse; healthCheckTimeout
   single: healthCheckTimeout

.. summary-start

Sets the timeout, in milliseconds, for verifying ClickHouse availability.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: healthCheckTimeout
:Scope: module
:Type: int (milliseconds)
:Default: module=3500
:Required?: no
:Introduced: not specified

Description
-----------
This parameter sets the timeout for checking the availability of ClickHouse. Value is given in milliseconds.

Module usage
------------
.. _param-omclickhouse-module-healthchecktimeout:
.. _omclickhouse.parameter.module.healthchecktimeout-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" healthCheckTimeout="5000")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
