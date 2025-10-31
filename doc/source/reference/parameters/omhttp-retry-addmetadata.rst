.. _param-omhttp-retry-addmetadata:
.. _omhttp.parameter.module.retry-addmetadata:

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
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: Not specified

Description
-----------
When this option is enabled, omhttp will add the response metadata to ``$!omhttp!response``. There are three response metadata fields added: ``code``, ``body``, ``batch_index``.

Module usage
------------
.. _param-omhttp-module-retry-addmetadata:
.. _omhttp.parameter.module.retry-addmetadata-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       retry="on"
       retry.ruleSet="rs_omhttp_retry"
       retry.addMetadata="on"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
