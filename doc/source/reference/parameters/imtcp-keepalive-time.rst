.. _param-imtcp-keepalive-time:
.. _imtcp.parameter.module.keepalive-time:
.. _imtcp.parameter.input.keepalive-time:

KeepAlive.Time
==============

.. index::
   single: imtcp; KeepAlive.Time
   single: KeepAlive.Time

.. summary-start

Sets the interval between last data and first keepalive probe.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: KeepAlive.Time
:Scope: module, input
:Type: integer
:Default: module=0, input=module parameter
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

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-keepalive-time:
.. _imtcp.parameter.module.keepalive-time-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive.time="...")

Input usage
-----------
.. _param-imtcp-input-keepalive-time:
.. _imtcp.parameter.input.keepalive-time-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" keepAlive.time="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverkeepalive_time:

- $InputTCPServerKeepAlive_time — maps to KeepAlive.Time (status: legacy)

.. index::
   single: imtcp; $InputTCPServerKeepAlive_time
   single: $InputTCPServerKeepAlive_time

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

