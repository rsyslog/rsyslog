.. _param-omelasticsearch-skippipelineifempty:
.. _omelasticsearch.parameter.module.skippipelineifempty:

skipPipelineIfEmpty
===================

.. index::
   single: omelasticsearch; skipPipelineIfEmpty
   single: skipPipelineIfEmpty

.. summary-start

Omit the pipeline parameter when `pipelineName` expands to an empty string.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: skipPipelineIfEmpty
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When enabled and the pipeline name resolves to an empty value, the parameter is not sent to Elasticsearch to avoid errors.

Action usage
------------
.. _param-omelasticsearch-action-skippipelineifempty:
.. _omelasticsearch.parameter.action.skippipelineifempty:
.. code-block:: rsyslog

   action(type="omelasticsearch" skipPipelineIfEmpty="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
