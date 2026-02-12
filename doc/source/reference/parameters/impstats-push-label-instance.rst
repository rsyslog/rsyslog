.. _param-impstats-push-label-instance:
.. _impstats.parameter.module.push-label-instance:

push.label.instance
===================

.. index::
   single: impstats; push.label.instance
   single: push.label.instance

.. summary-start

Controls whether impstats adds ``instance=<hostname>`` to pushed metrics.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.label.instance
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When enabled, impstats injects the local hostname as the ``instance`` label
unless one is already set via ``push.labels``.

Module usage
------------
.. _impstats.parameter.module.push-label-instance-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.label.instance="on")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
