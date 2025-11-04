.. _param-omclickhouse-port:
.. _omclickhouse.parameter.module.port:

Port
====

.. index::
   single: omclickhouse; Port
   single: Port

.. summary-start

Sets the HTTP port used to connect to the ClickHouse server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: Port
:Scope: module
:Type: integer
:Default: module=8123
:Required?: no
:Introduced: not specified

Description
-----------
HTTP port to use to connect to ClickHouse.

Module usage
------------
.. _param-omclickhouse-module-port:
.. _omclickhouse.parameter.module.port-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" port="9000")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
