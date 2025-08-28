.. _param-imptcp-keepalive-probes:
.. _imptcp.parameter.input.keepalive-probes:

KeepAlive.Probes
================

.. index::
   single: imptcp; KeepAlive.Probes
   single: KeepAlive.Probes

.. summary-start

Sets the number of unacknowledged keepalive probes before the connection is considered dead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: KeepAlive.Probes
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The number of unacknowledged probes to send before considering the
connection dead and notifying the application layer. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.

Input usage
-----------
.. _param-imptcp-input-keepalive-probes:
.. _imptcp.parameter.input.keepalive-probes-usage:

.. code-block:: rsyslog

   input(type="imptcp" keepAlive.probes="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverkeepalive_probes:

- $InputPTCPServerKeepAlive_probes — maps to KeepAlive.Probes (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerKeepAlive_probes
   single: $InputPTCPServerKeepAlive_probes

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
