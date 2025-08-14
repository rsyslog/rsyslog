.. _param-imtcp-keepalive-probes:
.. _imtcp.parameter.module.keepalive-probes:
.. _imtcp.parameter.input.keepalive-probes:

KeepAlive.Probes
================

.. index::
   single: imtcp; KeepAlive.Probes
   single: KeepAlive.Probes

.. summary-start

Sets the number of unacknowledged probes to send before a connection is considered dead.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: KeepAlive.Probes
:Scope: module, input
:Type: integer
:Default: ``0`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter sets the number of unacknowledged keep-alive probes to send before considering the connection dead and notifying the application layer. A value of ``0`` (the default) means that the operating system's default setting is used.

This parameter only has an effect if TCP keep-alive is enabled via the ``keepAlive`` parameter. The functionality may not be available on all platforms.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.keepalive-probes-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive="on" keepAlive.probes="3")

Input usage
-----------
.. _imtcp.parameter.input.keepalive-probes-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" keepAlive="on" keepAlive.probes="5")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverkeepalive_probes:
- $InputTCPServerKeepAlive_probes — maps to KeepAlive.Probes (status: legacy)

.. index::
   single: imtcp; $InputTCPServerKeepAlive_probes
   single: $InputTCPServerKeepAlive_probes

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
