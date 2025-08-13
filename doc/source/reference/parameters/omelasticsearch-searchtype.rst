.. _param-omelasticsearch-searchtype:
.. _omelasticsearch.parameter.module.searchtype:

searchType
==========

.. index::
   single: omelasticsearch; searchType
   single: searchType

.. summary-start

Elasticsearch type to use; empty string omits the type.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: searchType
:Scope: action
:Type: word
:Default: action=events
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Specifies the document type. Set to an empty string to omit the type which is required for Elasticsearch 7 and later.

Action usage
------------
.. _param-omelasticsearch-action-searchtype:
.. _omelasticsearch.parameter.action.searchtype:
.. code-block:: rsyslog

   action(type="omelasticsearch" searchType="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
