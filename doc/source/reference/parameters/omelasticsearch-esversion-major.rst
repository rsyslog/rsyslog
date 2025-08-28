.. _param-omelasticsearch-esversion-major:
.. _omelasticsearch.parameter.module.esversion-major:
.. _omelasticsearch.parameter.module.esVersion.major:

esVersion.major
===============

.. index::
   single: omelasticsearch; esVersion.major
   single: esVersion.major

.. summary-start

Elasticsearch major version used to select compatible APIs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: esVersion.major
:Scope: action
:Type: integer
:Default: action=0
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Value indicates the Elasticsearch major version (e.g. 7 or 8) and guides which API variant is used. Only value 8 currently changes behaviour.

Action usage
------------
.. _param-omelasticsearch-action-esversion-major:
.. _omelasticsearch.parameter.action.esversion-major:
.. code-block:: rsyslog

   action(type="omelasticsearch" esVersion.major="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
