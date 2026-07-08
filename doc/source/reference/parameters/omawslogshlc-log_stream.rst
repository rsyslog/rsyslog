.. _param-omawslogshlc-log_stream:
.. _omawslogshlc.parameter.action.log_stream:

.. meta::
   :description: Reference for the omawslogshlc log_stream parameter.
   :keywords: rsyslog, omawslogshlc, log_stream, aws, cloudwatch

log_stream
==========

.. index::
   single: omawslogshlc; log_stream
   single: log_stream

.. summary-start

Specifies the CloudWatch Logs log stream name within the log group.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omawslogshlc`.

:Name: log_stream
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``log_stream`` identifies the target log stream within the configured log
group. The value is URL-encoded automatically when constructing the HLC
endpoint URL.

Action usage
------------
.. _omawslogshlc.parameter.action.log_stream-usage:

.. code-block:: rsyslog

   action(type="omawslogshlc" log_stream="server-01" ...)

See also
--------
See also :doc:`../../configuration/modules/omawslogshlc`.
