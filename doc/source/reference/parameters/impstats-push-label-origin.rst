.. _param-impstats-push-label-origin:
.. _impstats.parameter.module.push-label-origin:

push.label.origin
=================

.. index::
   single: impstats; push.label.origin
   single: push.label.origin

.. summary-start

Controls whether impstats adds the stats origin as an ``origin`` label.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.label.origin
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When enabled, impstats injects the statsobj origin as the ``origin`` label
unless one is already set via ``push.labels``.

Module usage
------------
.. _impstats.parameter.module.push-label-origin-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.label.origin="on")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
