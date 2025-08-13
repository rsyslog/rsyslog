.. _param-omelasticsearch-indextimeout:
.. _omelasticsearch.parameter.module.indextimeout:

indexTimeout
============

.. index::
   single: omelasticsearch; indexTimeout
   single: indexTimeout

.. summary-start

Milliseconds to wait for an indexing request to complete.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: indexTimeout
:Scope: action
:Type: integer
:Default: action=0
:Required?: no
:Introduced: 8.2204.0

Description
-----------
Sets the client-side timeout for a log indexing request. `0` means no timeout.

Action usage
------------
.. _param-omelasticsearch-action-indextimeout:
.. _omelasticsearch.parameter.action.indextimeout:
.. code-block:: rsyslog

   action(type="omelasticsearch" indexTimeout="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
