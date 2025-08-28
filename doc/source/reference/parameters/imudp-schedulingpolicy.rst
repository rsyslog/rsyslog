.. _param-imudp-schedulingpolicy:
.. _imudp.parameter.module.schedulingpolicy:

SchedulingPolicy
================

.. index::
   single: imudp; SchedulingPolicy
   single: SchedulingPolicy

.. summary-start

Selects OS scheduler policy like ``fifo`` for real-time handling.

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
Can be used to set the scheduler priority, if the necessary functionality is
provided by the platform. Most useful to select ``fifo`` for real-time processing
under Linux (and thus reduce chance of packet loss). Other options are ``rr`` and
``other``.

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
