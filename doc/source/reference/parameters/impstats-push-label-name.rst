.. _param-impstats-push-label-name:
.. _impstats.parameter.module.push-label-name:

push.label.name
===============

.. index::
   single: impstats; push.label.name
   single: push.label.name

.. summary-start

Controls whether impstats adds the stats object name as a ``name`` label.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.label.name
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When enabled, impstats injects the statsobj name as the ``name`` label unless
one is already set via ``push.labels``.

Module usage
------------
.. _impstats.parameter.module.push-label-name-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.label.name="on")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
