.. _param-imudp-schedulingpriority:
.. _imudp.parameter.module.schedulingpriority:

SchedulingPriority
==================

.. index::
   single: imudp; SchedulingPriority
   single: SchedulingPriority

.. summary-start

Sets the numeric scheduler priority used with the selected scheduling policy.

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
Defines the priority value applied when a real-time scheduling policy like `fifo` or `rr` is chosen.

Module usage
------------
.. _param-imudp-module-schedulingpriority:
.. _imudp.parameter.module.schedulingpriority-usage:
.. code-block:: rsyslog

   module(load="imudp" SchedulingPriority="...")

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
