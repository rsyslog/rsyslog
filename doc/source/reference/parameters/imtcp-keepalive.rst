.. _param-imtcp-keepalive:
.. _imtcp.parameter.module.keepalive:
.. _imtcp.parameter.input.keepalive:

KeepAlive
=========

.. index::
   single: imtcp; KeepAlive
   single: KeepAlive

.. summary-start

Enables or disables TCP keep-alive packets on the listening socket.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: KeepAlive
:Scope: module, input
:Type: boolean
:Default: ``off`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This setting enables or disables keep-alive packets at the TCP socket layer. When enabled, the operating system periodically sends probes to the remote peer to ensure the connection is still active.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.keepalive-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive="on")

Input usage
-----------
.. _imtcp.parameter.input.keepalive-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" keepAlive="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverkeepalive:
- $InputTCPServerKeepAlive — maps to KeepAlive (status: legacy)

.. index::
   single: imtcp; $InputTCPServerKeepAlive
   single: $InputTCPServerKeepAlive

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
