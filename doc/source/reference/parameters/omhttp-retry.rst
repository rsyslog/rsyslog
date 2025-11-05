.. _param-omhttp-retry:
.. _omhttp.parameter.input.retry:

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
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter specifies whether failed requests should be retried using the custom retry logic implemented in this plugin. Requests returning 5XX HTTP status codes are considered retriable. If retry is enabled, set :ref:`param-omhttp-retry-ruleset` as well.

Note that retries are generally handled in rsyslog by setting ``action.resumeRetryCount="-1"`` (or some other integer), and the plugin lets rsyslog know it should start retrying by suspending itself. This is still the recommended approach in the two cases enumerated below when using this plugin. In both of these cases, the output plugin transaction interface is not used. That is, from rsyslog core's point of view, each message is contained in its own transaction.

1. Batching is off (``batch="off"``)
2. Batching is on and the maximum batch size is 1 (``batch="on" batchMaxSize="1"``)

This custom retry behavior is the result of a bug in rsyslog's handling of transaction commits. See `this issue <https://github.com/rsyslog/rsyslog/issues/2420>`_ for full details. Essentially, if rsyslog hands omhttp 4 messages, and omhttp batches them up but the request fails, rsyslog will only retry the LAST message that it handed the plugin, instead of all 4, even if the plugin returns the correct "defer commit" statuses for messages 1, 2, and 3. This means that omhttp cannot rely on ``action.resumeRetryCount`` for any transaction that processes more than a single message, and explains why the 2 above cases do work correctly.

It looks promising that issue will be resolved at some point, so this behavior can be revisited at that time.

Input usage
-----------
.. _omhttp.parameter.input.retry-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       retry="on"
       retryRuleSet="rs_omhttp_retry"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
