.. _param-imrelp-keepalive-probes:
.. _imrelp.parameter.input.keepalive-probes:

KeepAlive.Probes
================

.. index::
   single: imrelp; KeepAlive.Probes
   single: KeepAlive.Probes

.. summary-start

Defines how many keep-alive retries occur before the connection is declared dead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: KeepAlive.Probes
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: Not documented

Description
-----------
The number of keep-alive probes to send before considering the connection dead
and notifying the application layer. The default, 0, means that the operating
system defaults are used. This only has an effect if keep-alive is enabled. The
functionality may not be available on all platforms.

Input usage
-----------
.. _param-imrelp-input-keepalive-probes:
.. _imrelp.parameter.input.keepalive-probes-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" keepAlive="on" keepAlive.probes="5")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
