.. _param-omelasticsearch-dynsearchtype:
.. _omelasticsearch.parameter.module.dynsearchtype:

dynSearchType
=============

.. index::
   single: omelasticsearch; dynSearchType
   single: dynSearchType

.. summary-start

Treat ``searchType`` as a template that expands to the ``search_type`` value.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: dynSearchType
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When enabled, ``searchType`` references a template whose expansion is used as
the ``search_type`` query parameter.  Each expansion must evaluate to either
``query_then_fetch`` or ``dfs_query_then_fetch``; any other value is ignored and
generates an error message.

Action usage
------------
.. _param-omelasticsearch-action-dynsearchtype:
.. _omelasticsearch.parameter.action.dynsearchtype:
.. code-block:: rsyslog

   action(type="omelasticsearch" dynSearchType="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
