.. _param-omelasticsearch-pipelinename:
.. _omelasticsearch.parameter.module.pipelinename:

pipelineName
============

.. index::
   single: omelasticsearch; pipelineName
   single: pipelineName

.. summary-start

Ingest pipeline to run before indexing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: pipelineName
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Adds an ingest pipeline name to the request so events are pre-processed before indexing. By default no pipeline is used.

Action usage
------------
.. _param-omelasticsearch-action-pipelinename:
.. _omelasticsearch.parameter.action.pipelinename:
.. code-block:: rsyslog

   action(type="omelasticsearch" pipelineName="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
