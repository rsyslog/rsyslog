.. _param-omhttp-retry-addmetadata:
.. _omhttp.parameter.input.retry-addmetadata:

retry.addmetadata
=================

.. index::
   single: omhttp; retry.addmetadata
   single: retry.addmetadata

.. summary-start

Adds HTTP response metadata to ``$!omhttp!response`` for messages handled by the retry logic.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: retry.addmetadata
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not specified

Description
-----------
When this option is enabled, omhttp will add the response metadata to ``$!omhttp!response``. There are three response metadata fields added: ``code``, ``body``, ``batch_index``.

Input usage
-----------
.. _omhttp.parameter.input.retry-addmetadata-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       retry="on"
       retryRuleSet="rs_omhttp_retry"
       retryAddMetadata="on"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
