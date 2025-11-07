.. _param-omclickhouse-server:
.. _omclickhouse.parameter.input.server:

server
======

.. index::
   single: omclickhouse; server
   single: server

.. summary-start

Specifies the address of the ClickHouse server that receives events from this
action.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: server
:Scope: input
:Type: word
:Default: localhost
:Required?: no
:Introduced: not specified

Description
-----------
The address of a ClickHouse server.

Input usage
-----------
.. _omclickhouse.parameter.input.server-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" server="clickhouse.example.com")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
