.. _param-imudp-schedulingpriority:
.. _imudp.parameter.module.schedulingpriority:

SchedulingPriority
==================

.. index::
   single: imudp; SchedulingPriority
   single: SchedulingPriority

.. summary-start

Specifies the scheduler priority value when a policy like ``fifo`` is used.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: SchedulingPriority
:Scope: module
:Type: integer
:Default: module=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Defines the scheduler priority to apply when a real-time scheduling policy is
selected.

Module usage
------------
.. _param-imudp-module-schedulingpriority:
.. _imudp.parameter.module.schedulingpriority-usage:
.. code-block:: rsyslog

   module(load="imudp" SchedulingPriority="...")

Notes
-----
- Scheduling parameters are set after privilege drop and may fail without sufficient privileges.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imudp.parameter.legacy.imudpschedulingpriority:
- $IMUDPSchedulingPriority — maps to SchedulingPriority (status: legacy)

.. index::
   single: imudp; $IMUDPSchedulingPriority
   single: $IMUDPSchedulingPriority

See also
--------
See also :doc:`../../configuration/modules/imudp`.

