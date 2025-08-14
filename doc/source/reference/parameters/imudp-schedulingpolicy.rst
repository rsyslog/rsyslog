.. _param-imudp-schedulingpolicy:
.. _imudp.parameter.module.schedulingpolicy:

SchedulingPolicy
================

.. index::
   single: imudp; SchedulingPolicy
   single: SchedulingPolicy

.. summary-start

Selects the thread scheduling policy such as ``fifo`` for real-time processing.

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
Can set the scheduler policy if the platform supports it. "fifo" is most useful
for real-time processing under Linux to reduce packet loss. Other valid options
are "rr" and "other".

Module usage
------------
.. _param-imudp-module-schedulingpolicy:
.. _imudp.parameter.module.schedulingpolicy-usage:
.. code-block:: rsyslog

   module(load="imudp" SchedulingPolicy="...")

Notes
-----
- Scheduling parameters are set after privilege drop and may fail without sufficient privileges.

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

