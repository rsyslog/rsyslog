.. _param-omelasticsearch-retryruleset:
.. _omelasticsearch.parameter.module.retryruleset:

retryruleset
============

.. index::
   single: omelasticsearch; retryruleset
   single: retryruleset

.. summary-start

Ruleset used when processing retried records.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: retryruleset
:Scope: action
:Type: word
:Default: none (unset)
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Names a ruleset to handle records requeued by `retryfailures`. If unset, retries start at the default ruleset.

Action usage
------------
.. _param-omelasticsearch-action-retryruleset:
.. _omelasticsearch.parameter.action.retryruleset:
.. code-block:: rsyslog

   action(type="omelasticsearch" retryruleset="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
