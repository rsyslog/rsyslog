.. _param-omelasticsearch-searchindex:
.. _omelasticsearch.parameter.module.searchindex:

searchIndex
===========

.. index::
   single: omelasticsearch; searchIndex
   single: searchIndex

.. summary-start

Elasticsearch index where events are written.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: searchIndex
:Scope: action
:Type: word
:Default: action=system
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Name of the target Elasticsearch index. When unset it defaults to `system`.

Action usage
------------
.. _param-omelasticsearch-action-searchindex:
.. _omelasticsearch.parameter.action.searchindex:
.. code-block:: rsyslog

   action(type="omelasticsearch" searchIndex="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
