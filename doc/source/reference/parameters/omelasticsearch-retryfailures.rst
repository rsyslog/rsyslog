.. _param-omelasticsearch-retryfailures:
.. _omelasticsearch.parameter.module.retryfailures:

retryfailures
=============

.. index::
   single: omelasticsearch; retryfailures
   single: retryfailures

.. summary-start

Resubmit failed bulk items back into rsyslog for retry.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: retryfailures
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When enabled, bulk responses are scanned for errors and failed entries are resubmitted with metadata under `$.omes`.

Action usage
------------
.. _param-omelasticsearch-action-retryfailures:
.. _omelasticsearch.parameter.action.retryfailures:
.. code-block:: rsyslog

   action(type="omelasticsearch" retryfailures="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
