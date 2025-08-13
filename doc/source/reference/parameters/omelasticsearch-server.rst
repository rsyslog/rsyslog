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
Defines one or more Elasticsearch servers. If no scheme is given, it is chosen based on `usehttps`. Missing ports use `serverport`. Requests are load-balanced in round-robin order.

Action usage
------------
.. _param-omelasticsearch-action-server:
.. _omelasticsearch.parameter.action.server:
.. code-block:: rsyslog

   action(type="omelasticsearch" Server="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
