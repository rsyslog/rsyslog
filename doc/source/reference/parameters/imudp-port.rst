.. _param-imudp-port:
.. _imudp.parameter.module.port:

Port
====

.. index::
   single: imudp; Port
   single: Port

.. summary-start

Specifies one or more UDP ports that `imudp` listens on.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Port
:Scope: input
:Type: array[string]
:Default: input=514
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
Defines the UDP port or list of ports to bind; when multiple ports are given, a listener is started for each one automatically.

Input usage
-----------
.. _param-imudp-input-port:
.. _imudp.parameter.input.port:
.. code-block:: rsyslog

   input(type="imudp" Port="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imudp.parameter.legacy.udpserverrun:

- $UDPServerRun — maps to Port (status: legacy)

.. index::
   single: imudp; $UDPServerRun
   single: $UDPServerRun

See also
--------
See also :doc:`../../configuration/modules/imudp`.
