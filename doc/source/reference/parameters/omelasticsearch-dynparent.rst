.. _param-omelasticsearch-dynparent:
.. _omelasticsearch.parameter.module.dynparent:

dynParent
=========

.. index::
   single: omelasticsearch; dynParent
   single: dynParent

.. summary-start

Treat `parent` as a template for per-record parent IDs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: dynParent
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When enabled, `parent` names a template that produces the parent ID for each record.

Action usage
------------
.. _param-omelasticsearch-action-dynparent:
.. _omelasticsearch.parameter.action.dynparent:
.. code-block:: rsyslog

   action(type="omelasticsearch" dynParent="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
