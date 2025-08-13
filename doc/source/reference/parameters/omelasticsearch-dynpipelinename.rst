.. _param-omelasticsearch-dynpipelinename:
.. _omelasticsearch.parameter.module.dynpipelinename:

dynPipelineName
===============

.. index::
   single: omelasticsearch; dynPipelineName
   single: dynPipelineName

.. summary-start

Treat `pipelineName` as a template name.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: dynPipelineName
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
If enabled, the value of `pipelineName` is taken as a template whose result selects the pipeline.

Action usage
------------
.. _param-omelasticsearch-action-dynpipelinename:
.. _omelasticsearch.parameter.action.dynpipelinename:
.. code-block:: rsyslog

   action(type="omelasticsearch" dynPipelineName="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
