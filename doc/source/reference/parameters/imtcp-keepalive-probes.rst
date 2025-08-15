.. _param-imtcp-keepalive-probes:
.. _imtcp.parameter.module.keepalive-probes:
.. _imtcp.parameter.input.keepalive-probes:

KeepAlive.Probes
================

.. index::
   single: imtcp; KeepAlive.Probes
   single: KeepAlive.Probes

.. summary-start

Defines how many unacknowledged probes are sent before a connection is considered dead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: KeepAlive.Probes
:Scope: module, input
:Type: integer
:Default: module=0, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The number of unacknowledged probes to send before considering the
connection dead and notifying the application layer. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-keepalive-probes:
.. _imtcp.parameter.module.keepalive-probes-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive.probes="...")

Input usage
-----------
.. _param-imtcp-input-keepalive-probes:
.. _imtcp.parameter.input.keepalive-probes-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" keepAlive.probes="...")

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

