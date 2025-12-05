.. _param-omelasticsearch-server:
.. _omelasticsearch.parameter.module.server:

Server
======

.. index::
   single: omelasticsearch; Server
   single: Server

.. summary-start

List of Elasticsearch servers to send events to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: Server
:Scope: action
:Type: array[string]
:Default: action=localhost
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Defines one or more Elasticsearch servers. Each entry may be a bare hostname or an ``http``/``https`` URL without a trailing slash or path component. If no scheme is given, one is selected based on `usehttps`, otherwise the provided scheme is honored. Missing ports use `serverport`. Requests are load-balanced in round-robin order.

Action usage
------------
.. _param-omelasticsearch-action-server:
.. _omelasticsearch.parameter.action.server:
.. code-block:: rsyslog

   action(type="omelasticsearch" Server="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
