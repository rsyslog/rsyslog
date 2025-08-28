.. _param-omelasticsearch-dynbulkid:
.. _omelasticsearch.parameter.module.dynbulkid:

dynbulkid
=========

.. index::
   single: omelasticsearch; dynbulkid
   single: dynbulkid

.. summary-start

Treat `bulkid` as a template that generates per-record IDs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: dynbulkid
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Enables interpreting `bulkid` as a template, producing a unique identifier for each record.

Action usage
------------
.. _param-omelasticsearch-action-dynbulkid:
.. _omelasticsearch.parameter.action.dynbulkid:
.. code-block:: rsyslog

   action(type="omelasticsearch" dynbulkid="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
