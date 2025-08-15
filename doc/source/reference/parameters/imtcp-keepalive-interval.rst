.. _param-imtcp-keepalive-interval:
.. _imtcp.parameter.module.keepalive-interval:
.. _imtcp.parameter.input.keepalive-interval:

KeepAlive.Interval
==================

.. index::
   single: imtcp; KeepAlive.Interval
   single: KeepAlive.Interval

.. summary-start

Defines the interval for keep-alive packets.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: KeepAlive.Interval
:Scope: module, input
:Type: integer
:Default: module=0, input=module parameter
:Required?: no
:Introduced: 8.2106.0

Description
-----------
The interval for keep alive packets.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-keepalive-interval:
.. _imtcp.parameter.module.keepalive-interval-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive.interval="...")

Input usage
-----------
.. _param-imtcp-input-keepalive-interval:
.. _imtcp.parameter.input.keepalive-interval-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" keepAlive.interval="...")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

