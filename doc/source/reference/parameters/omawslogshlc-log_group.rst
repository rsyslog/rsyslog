.. _param-omawslogshlc-log_group:
.. _omawslogshlc.parameter.action.log_group:

.. meta::
   :description: Reference for the omawslogshlc log_group parameter.
   :keywords: rsyslog, omawslogshlc, log_group, aws, cloudwatch

log_group
=========

.. index::
   single: omawslogshlc; log_group
   single: log_group

.. summary-start

Specifies the CloudWatch Logs log group name to send events to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omawslogshlc`.

:Name: log_group
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``log_group`` identifies the target CloudWatch Logs log group. The value
is URL-encoded automatically when constructing the HLC endpoint URL.

Action usage
------------
.. _omawslogshlc.parameter.action.log_group-usage:

.. code-block:: rsyslog

   action(type="omawslogshlc" log_group="my-application-logs" ...)

See also
--------
See also :doc:`../../configuration/modules/omawslogshlc`.
