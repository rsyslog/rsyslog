.. _param-imptcp-keepalive-interval:
.. _imptcp.parameter.input.keepalive-interval:

KeepAlive.Interval
==================

.. index::
   single: imptcp; KeepAlive.Interval
   single: KeepAlive.Interval

.. summary-start

Sets the interval between keepalive probes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: KeepAlive.Interval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The interval between subsequential keepalive probes, regardless of
what the connection has exchanged in the meantime. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.

Input usage
-----------
.. _param-imptcp-input-keepalive-interval:
.. _imptcp.parameter.input.keepalive-interval-usage:

.. code-block:: rsyslog

   input(type="imptcp" keepAlive.interval="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverkeepalive_intvl:

- $InputPTCPServerKeepAlive_intvl — maps to KeepAlive.Interval (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerKeepAlive_intvl
   single: $InputPTCPServerKeepAlive_intvl

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
