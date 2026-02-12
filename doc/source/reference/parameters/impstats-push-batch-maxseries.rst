.. _param-impstats-push-batch-maxseries:
.. _impstats.parameter.module.push-batch-maxseries:

push.batch.maxSeries
====================

.. index::
   single: impstats; push.batch.maxSeries
   single: push.batch.maxSeries

.. summary-start

Limits the number of time series per Remote Write request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.batch.maxSeries
:Scope: module
:Type: integer (count)
:Default: module=0
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When set, impstats splits the payload into multiple requests, each containing
at most the specified number of series.

Module usage
------------
.. _impstats.parameter.module.push-batch-maxseries-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.batch.maxSeries="1000")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
