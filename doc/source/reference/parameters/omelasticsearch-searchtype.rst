.. _param-omelasticsearch-searchtype:
.. _omelasticsearch.parameter.module.searchtype:

searchType
==========

.. index::
   single: omelasticsearch; searchType
   single: searchType

.. summary-start

Appends the ``search_type`` query parameter to REST requests when set.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: searchType
:Scope: action
:Type: word
:Default: action=(omitted)
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Controls the ``search_type`` query parameter sent with each REST request.  Only
``query_then_fetch`` and ``dfs_query_then_fetch`` are supported values.  Any
other value is ignored and an error is written to the log so the configuration
can be corrected.  When the remote service identifies itself as Elasticsearch,
the parameter is suppressed regardless of configuration because recent
Elasticsearch versions reject it; an error message is emitted in this case as
well.  The option remains available for OpenSearch clusters which still accept
``search_type``.

Action usage
------------
.. _param-omelasticsearch-action-searchtype:
.. _omelasticsearch.parameter.action.searchtype:
.. code-block:: rsyslog

   action(type="omelasticsearch" searchType="dfs_query_then_fetch")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
