.. _param-omelasticsearch-dynsearchindex:
.. _omelasticsearch.parameter.module.dynsearchindex:

dynSearchIndex
==============

.. index::
   single: omelasticsearch; dynSearchIndex
   single: dynSearchIndex

.. summary-start

Treat `searchIndex` as a template name instead of a literal.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: dynSearchIndex
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
If enabled, the value of `searchIndex` is interpreted as the name of a template whose expansion becomes the index name.

Action usage
------------
.. _param-omelasticsearch-action-dynsearchindex:
.. _omelasticsearch.parameter.action.dynsearchindex:
.. code-block:: rsyslog

   action(type="omelasticsearch" dynSearchIndex="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
