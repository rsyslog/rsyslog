.. _param-omelasticsearch-maxbytes:
.. _omelasticsearch.parameter.module.maxbytes:

maxbytes
========

.. index::
   single: omelasticsearch; maxbytes
   single: maxbytes

.. summary-start

Maximum size of a bulk request body when bulkmode is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: maxbytes
:Scope: action
:Type: word
:Default: action=100m
:Required?: no
:Introduced: 8.23.0

Description
-----------
Events are batched until this size or the dequeue batch size is reached. Set to match Elasticsearch `http.max_content_length`.

Action usage
------------
.. _param-omelasticsearch-action-maxbytes:
.. _omelasticsearch.parameter.action.maxbytes:
.. code-block:: rsyslog

   action(type="omelasticsearch" maxbytes="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
