.. _param-imptcp-keepalive:
.. _imptcp.parameter.input.keepalive:

KeepAlive
=========

.. index::
   single: imptcp; KeepAlive
   single: KeepAlive

.. summary-start

Enables TCP keep-alive packets on the socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: KeepAlive
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Enable or disable keep-alive packets at the tcp socket layer. The
default is to disable them.

Input usage
-----------
.. _param-imptcp-input-keepalive:
.. _imptcp.parameter.input.keepalive-usage:

.. code-block:: rsyslog

   input(type="imptcp" keepAlive="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverkeepalive:

- $InputPTCPServerKeepAlive — maps to KeepAlive (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerKeepAlive
   single: $InputPTCPServerKeepAlive

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
