.. _param-omelasticsearch-writeoperation:
.. _omelasticsearch.parameter.module.writeoperation:

writeoperation
==============

.. index::
   single: omelasticsearch; writeoperation
   single: writeoperation

.. summary-start

Bulk action type: `index` (default) or `create` to avoid overwrites.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: writeoperation
:Scope: action
:Type: word
:Default: action=index
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Controls the operation sent in bulk requests. Use `create` to only add documents that do not yet exist. Requires `bulkid` and `dynbulkid`.

Action usage
------------
.. _param-omelasticsearch-action-writeoperation:
.. _omelasticsearch.parameter.action.writeoperation:
.. code-block:: rsyslog

   action(type="omelasticsearch" writeoperation="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
