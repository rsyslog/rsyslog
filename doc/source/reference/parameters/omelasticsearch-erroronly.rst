.. _param-omelasticsearch-erroronly:
.. _omelasticsearch.parameter.module.erroronly:

errorOnly
=========

.. index::
   single: omelasticsearch; errorOnly
   single: errorOnly

.. summary-start

Limit bulk error-file output to failed records.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: errorOnly
:Scope: action
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When enabled, omelasticsearch writes only records that Elasticsearch reported
as failed to the configured :ref:`param-omelasticsearch-errorfile`.

This option affects the format of the error file in bulk mode. Without it,
the error file includes the full bulk request and response context for a
failed bulk operation. With ``errorOnly="on"``, entries that succeeded inside
the same bulk response are omitted from the error-file payload.

Action usage
------------
.. _param-omelasticsearch-action-erroronly:
.. _omelasticsearch.parameter.action.erroronly:
.. code-block:: rsyslog

   action(type="omelasticsearch" errorFile="..." errorOnly="on")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`,
:ref:`param-omelasticsearch-errorfile`, and
:ref:`param-omelasticsearch-interleaved`.
