.. _param-omawslogshlc-max_batch_size:
.. _omawslogshlc.parameter.action.max_batch_size:

.. meta::
   :description: Reference for the omawslogshlc max_batch_size parameter.
   :keywords: rsyslog, omawslogshlc, max_batch_size, aws, cloudwatch, batching

max_batch_size
==============

.. index::
   single: omawslogshlc; max_batch_size
   single: max_batch_size

.. summary-start

Specifies the maximum number of events per HTTP request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omawslogshlc`.

:Name: max_batch_size
:Scope: action
:Type: integer
:Default: 100
:Required?: no
:Introduced: Not specified

Description
-----------
``max_batch_size`` controls how many events are accumulated before the
batch is flushed to CloudWatch Logs. Valid range is 1 to 10000. AWS
recommends 10 to 100 events per request for optimal performance.

Action usage
------------
.. _omawslogshlc.parameter.action.max_batch_size-usage:

.. code-block:: rsyslog

   action(type="omawslogshlc" max_batch_size="50" ...)

See also
--------
See also :doc:`../../configuration/modules/omawslogshlc`.
