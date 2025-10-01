.. _param-omelasticsearch-ratelimit-interval:
.. _omelasticsearch.parameter.module.ratelimit-interval:
.. _omelasticsearch.parameter.module.ratelimit.interval:

ratelimit.interval
==================

.. index::
   single: omelasticsearch; ratelimit.interval
   single: ratelimit.interval

.. summary-start

Seconds over which retry rate limiting is calculated.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: ratelimit.interval
:Scope: action
:Type: integer
:Default: action=600
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Defines the interval for rate limiting when `retryfailures` is enabled. Set to `0` to disable.

Action usage
------------
.. _param-omelasticsearch-action-ratelimit-interval:
.. _omelasticsearch.parameter.action.ratelimit-interval:
.. code-block:: rsyslog

   action(type="omelasticsearch" ratelimit.interval="...")

Notes
-----

.. include:: ratelimit-inline-note.rst

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
