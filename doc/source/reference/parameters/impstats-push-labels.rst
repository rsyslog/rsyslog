.. _param-impstats-push-labels:
.. _impstats.parameter.module.push-labels:

push.labels
===========

.. index::
   single: impstats; push.labels
   single: push.labels

.. summary-start

Adds static labels (``key=value``) to every pushed metric.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.labels
:Scope: module
:Type: array (strings)
:Default: module=none
:Required?: no
:Introduced: 8.2602.0

Description
-----------
Defines an array of labels that will be attached to all metrics. Each entry must
be a ``key=value`` string.

Module usage
------------
.. _impstats.parameter.module.push-labels-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.labels=["env=dev", "instance=rsyslog-1"])

See also
--------
See also :doc:`../../configuration/modules/impstats`.
