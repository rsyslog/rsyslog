.. _param-imrelp-keepalive-interval:
.. _imrelp.parameter.input.keepalive-interval:

keepAlive.interval
==================

.. index::
   single: imrelp; keepAlive.interval
   single: keepAlive.interval

.. summary-start

Determines the delay between successive keep-alive probes when enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: keepAlive.interval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 8.2108.0, possibly earlier

Description
-----------
The interval between subsequent keep-alive probes, regardless of what the
connection has been exchanged in the meantime. The default, 0, means that the
operating system defaults are used. This only has an effect if keep-alive is
enabled. The functionality may not be available on all platforms.

Input usage
-----------
.. _param-imrelp-input-keepalive-interval-usage:
.. _imrelp.parameter.input.keepalive-interval-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" keepAlive="on" keepAlive.interval="30")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
