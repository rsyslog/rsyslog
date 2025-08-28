.. _param-omelasticsearch-asyncrepl:
.. _omelasticsearch.parameter.module.asyncrepl:

asyncrepl
=========

.. index::
   single: omelasticsearch; asyncrepl
   single: asyncrepl

.. summary-start

Deprecated option formerly enabling asynchronous replication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: asyncrepl
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Kept for compatibility; Elasticsearch removed support for asynchronous replication so this setting has no effect.

Action usage
------------
.. _param-omelasticsearch-action-asyncrepl:
.. _omelasticsearch.parameter.action.asyncrepl:
.. code-block:: rsyslog

   action(type="omelasticsearch" asyncrepl="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
