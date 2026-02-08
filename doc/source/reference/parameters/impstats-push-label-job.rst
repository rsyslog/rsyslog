.. _param-impstats-push-label-job:
.. _impstats.parameter.module.push-label-job:

push.label.job
==============

.. index::
   single: impstats; push.label.job
   single: push.label.job

.. summary-start

Adds a ``job=<value>`` label to pushed metrics.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.label.job
:Scope: module
:Type: word
:Default: module=rsyslog
:Required?: no
:Introduced: 8.2602.0

Description
-----------
Sets the ``job`` label used by Prometheus conventions. Set to an empty string
to disable. If a ``job`` label already exists in ``push.labels``, it is not
overridden.

Module usage
------------
.. _impstats.parameter.module.push-label-job-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.label.job="rsyslog")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
