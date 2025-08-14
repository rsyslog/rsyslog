.. _param-imudp-schedulingpolicy:
.. _imudp.parameter.module.schedulingpolicy:

SchedulingPolicy
================

.. index::
   single: imudp; SchedulingPolicy
   single: SchedulingPolicy

.. summary-start

Sets the scheduler policy such as `fifo`, `rr` or `other` for UDP listener threads.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: SchedulingPolicy
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Selects the scheduling policy for listener threads to influence kernel scheduling and reduce packet loss under heavy load.

Module usage
------------
.. _param-imudp-module-schedulingpolicy:
.. _imudp.parameter.module.schedulingpolicy-usage:
.. code-block:: rsyslog

   module(load="imudp" SchedulingPolicy="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imudp.parameter.legacy.imudpschedulingpolicy:

- $IMUDPSchedulingPolicy — maps to SchedulingPolicy (status: legacy)

.. index::
   single: imudp; $IMUDPSchedulingPolicy
   single: $IMUDPSchedulingPolicy

See also
--------
See also :doc:`../../configuration/modules/imudp`.
