.. _param-omelasticsearch-healthchecktimeout:
.. _omelasticsearch.parameter.module.healthchecktimeout:

HealthCheckTimeout
==================

.. index::
   single: omelasticsearch; HealthCheckTimeout
   single: HealthCheckTimeout

.. summary-start

Milliseconds to wait for an HTTP health check before sending events.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: HealthCheckTimeout
:Scope: action
:Type: integer
:Default: action=3500
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Before logging begins, rsyslog issues an HTTP HEAD to `/_cat/health` and waits up to this many milliseconds for `HTTP OK`.

Action usage
------------
.. _param-omelasticsearch-action-healthchecktimeout:
.. _omelasticsearch.parameter.action.healthchecktimeout:
.. code-block:: rsyslog

   action(type="omelasticsearch" HealthCheckTimeout="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
