.. _param-omhttp-retry-ruleset:
.. _omhttp.parameter.module.retry-ruleset:

retry.ruleset
=============

.. index::
   single: omhttp; retry.ruleset
   single: retry.ruleset

.. summary-start

Names the ruleset where omhttp requeues failed messages when :ref:`param-omhttp-retry` is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: retry.ruleset
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter specifies the ruleset where this plugin should requeue failed messages if :ref:`param-omhttp-retry` is on. This ruleset generally would contain another omhttp action instance.

**Important** - Note that the message that is queued on the retry ruleset is the templated output of the initial omhttp action. This means that no further templating should be done to messages inside this ruleset, unless retries should be templated differently than first-tries. An "echo template" does the trick here.

.. code-block:: text

   template(name="tpl_echo" type="string" string="%msg%")

This retry ruleset can recursively call itself as its own ``retry.ruleset`` to retry forever, but there is no timeout behavior currently implemented.

Alternatively, the omhttp action in the retry ruleset could be configured to support ``action.resumeRetryCount`` as explained above in the :ref:`param-omhttp-retry` section. The benefit of this approach is that retried messages still hit the server in a batch format (though with a single message in it), and the ability to configure rsyslog to give up after some number of resume attempts so as to avoid resource exhaustion.

Or, if some data loss or high latency is acceptable, do not configure retries with the retry ruleset itself. A single retry from the original ruleset might catch most failures, and errors from the retry ruleset could still be logged using the :ref:`param-omhttp-errorfile` parameter and sent later on via some other process.

Module usage
------------
.. _param-omhttp-module-retry-ruleset:
.. _omhttp.parameter.module.retry-ruleset-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   ruleset(name="rs_omhttp_retry") {
       action(
           type="omhttp"
           template="tpl_echo"
           batch="on"
           batch.format="jsonarray"
           batch.maxsize="5"
       )
   }

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
