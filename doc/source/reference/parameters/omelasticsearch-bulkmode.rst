.. _param-omelasticsearch-bulkmode:
.. _omelasticsearch.parameter.module.bulkmode:

bulkmode
========

.. index::
   single: omelasticsearch; bulkmode
   single: bulkmode

.. summary-start

Use the Elasticsearch Bulk API to send batched events.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: bulkmode
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
If enabled, events are buffered and sent in bulk requests. Without it, each event is sent individually.

Action usage
------------
.. _param-omelasticsearch-action-bulkmode:
.. _omelasticsearch.parameter.action.bulkmode:
.. code-block:: rsyslog

   action(type="omelasticsearch" bulkmode="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
