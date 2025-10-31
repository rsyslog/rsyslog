.. _param-omhttp-retry:
.. _omhttp.parameter.module.retry:

retry
=====

.. index::
   single: omhttp; retry
   single: retry

.. summary-start

Enables omhttp's internal retry logic that requeues failed requests for another attempt.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: retry
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter specifies whether failed requests should be retried using the custom retry logic implemented in this plugin. Requests returning 5XX HTTP status codes are considered retriable. If retry is enabled, set :ref:`param-omhttp-retry-ruleset` as well.

Note that retries are generally handled in rsyslog by setting ``action.resumeRetryCount="-1"`` (or some other integer), and the plugin lets rsyslog know it should start retrying by suspending itself. This is still the recommended approach in the two cases enumerated below when using this plugin. In both of these cases, the output plugin transaction interface is not used. That is, from rsyslog core's point of view, each message is contained in its own transaction.

Module usage
------------
.. _param-omhttp-module-retry:
.. _omhttp.parameter.module.retry-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       retry="on"
       retry.ruleSet="rs_omhttp_retry"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
