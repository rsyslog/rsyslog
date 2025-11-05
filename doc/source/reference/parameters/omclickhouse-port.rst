.. _param-omclickhouse-port:
.. _omclickhouse.parameter.input.port:

port
====

.. index::
   single: omclickhouse; port
   single: port
   single: omclickhouse; Port
   single: Port

.. summary-start

Sets the HTTP port used to connect to the ClickHouse server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: port
:Scope: input
:Type: integer
:Default: 8123
:Required?: no
:Introduced: not specified

Description
-----------
HTTP port to use to connect to ClickHouse.

Input usage
-----------
.. _omclickhouse.parameter.input.port-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" port="9000")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
