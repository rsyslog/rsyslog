.. _param-omclickhouse-server:
.. _omclickhouse.parameter.module.server:

server
======

.. index::
   single: omclickhouse; server
   single: server
   single: omclickhouse; Server
   single: Server

.. summary-start

Specifies the address of the ClickHouse server that receives events from this action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: server
:Scope: module
:Type: word
:Default: module=localhost
:Required?: no
:Introduced: not specified

Description
-----------
The address of a ClickHouse server.

Module usage
------------
.. _param-omclickhouse-module-server:
.. _omclickhouse.parameter.module.server-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" server="clickhouse.example.com")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
