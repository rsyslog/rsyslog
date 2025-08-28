.. _param-imptcp-keepalive-time:
.. _imptcp.parameter.input.keepalive-time:

KeepAlive.Time
==============

.. index::
   single: imptcp; KeepAlive.Time
   single: KeepAlive.Time

.. summary-start

Sets idle time before the first keepalive probe is sent.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: KeepAlive.Time
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The interval between the last data packet sent (simple ACKs are not
considered data) and the first keepalive probe; after the connection
is marked to need keepalive, this counter is not used any further.
The default, 0, means that the operating system defaults are used.
This has only effect if keep-alive is enabled. The functionality may
not be available on all platforms.

Input usage
-----------
.. _param-imptcp-input-keepalive-time:
.. _imptcp.parameter.input.keepalive-time-usage:

.. code-block:: rsyslog

   input(type="imptcp" keepAlive.time="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverkeepalive_time:

- $InputPTCPServerKeepAlive_time — maps to KeepAlive.Time (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerKeepAlive_time
   single: $InputPTCPServerKeepAlive_time

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
