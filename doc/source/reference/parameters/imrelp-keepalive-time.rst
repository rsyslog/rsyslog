.. _param-imrelp-keepalive-time:
.. _imrelp.parameter.input.keepalive-time:

keepAlive.time
==============

.. index::
   single: imrelp; keepAlive.time
   single: keepAlive.time

.. summary-start

Controls how long a connection stays idle before the first keep-alive probe is sent.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: keepAlive.time
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 8.2108.0, possibly earlier

Description
-----------
The interval between the last data packet sent (simple ACKs are not considered
data) and the first keepalive probe; after the connection is marked with
keep-alive, this counter is not used any further. The default, 0, means that the
operating system defaults are used. This only has an effect if keep-alive is
enabled. The functionality may not be available on all platforms.

Input usage
-----------
.. _param-imrelp-input-keepalive-time:
.. _imrelp.parameter.input.keepalive-time-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" keepAlive="on" keepAlive.time="600")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
