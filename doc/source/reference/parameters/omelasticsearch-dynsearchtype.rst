.. _param-omelasticsearch-dynsearchtype:
.. _omelasticsearch.parameter.module.dynsearchtype:

dynSearchType
=============

.. index::
   single: omelasticsearch; dynSearchType
   single: dynSearchType

.. summary-start

Treat `searchType` as a template name.

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
When enabled, `searchType` references a template whose expansion becomes the type.

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
