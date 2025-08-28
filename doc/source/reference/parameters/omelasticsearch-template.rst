.. _param-omelasticsearch-template:
.. _omelasticsearch.parameter.module.template:

template
========

.. index::
   single: omelasticsearch; template
   single: template

.. summary-start

Template used to render the JSON document sent to Elasticsearch.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: template
:Scope: action
:Type: word
:Default: action=StdJSONFmt
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Defines the rsyslog template that generates the JSON payload. The default `StdJSONFmt` includes common fields.

Action usage
------------
.. _param-omelasticsearch-action-template:
.. _omelasticsearch.parameter.action.template:
.. code-block:: rsyslog

   action(type="omelasticsearch" template="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
