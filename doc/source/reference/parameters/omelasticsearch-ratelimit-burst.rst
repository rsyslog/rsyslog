.. _param-omelasticsearch-ratelimit-burst:
.. _omelasticsearch.parameter.module.ratelimit-burst:
.. _omelasticsearch.parameter.module.ratelimit.burst:

ratelimit.burst
===============

.. index::
   single: omelasticsearch; ratelimit.burst
   single: ratelimit.burst

.. summary-start

Maximum messages allowed in a rate-limit interval.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: ratelimit.burst
:Scope: action
:Type: integer
:Default: action=20000
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Specifies the burst size permitted within `ratelimit.interval` when retry limiting is active.

Action usage
------------
.. _param-omelasticsearch-action-ratelimit-burst:
.. _omelasticsearch.parameter.action.ratelimit-burst:
.. code-block:: rsyslog

   action(type="omelasticsearch" ratelimit.burst="...")

Notes
-----

.. include:: ratelimit-inline-note.rst

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
