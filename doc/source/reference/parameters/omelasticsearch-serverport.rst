.. _param-omelasticsearch-serverport:
.. _omelasticsearch.parameter.module.serverport:

Serverport
==========

.. index::
   single: omelasticsearch; Serverport
   single: Serverport

.. summary-start

Default port used when server URLs omit a port.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: Serverport
:Scope: action
:Type: integer
:Default: action=9200
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Port applied to entries in `server` that lack an explicit port number.

Action usage
------------
.. _param-omelasticsearch-action-serverport:
.. _omelasticsearch.parameter.action.serverport:
.. code-block:: rsyslog

   action(type="omelasticsearch" Serverport="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
