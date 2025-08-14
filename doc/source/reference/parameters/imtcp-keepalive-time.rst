.. _param-imtcp-keepalive-time:
.. _imtcp.parameter.module.keepalive-time:
.. _imtcp.parameter.input.keepalive-time:

KeepAlive.Time
==============

.. index::
   single: imtcp; KeepAlive.Time
   single: KeepAlive.Time

.. summary-start

Sets the time interval before the first keep-alive probe is sent.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: KeepAlive.Time
:Scope: module, input
:Type: integer
:Default: ``0`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter defines the interval (in seconds) between the last data packet sent and the first keep-alive probe. After the connection is marked to need keep-alive, this counter is not used any further. A value of ``0`` (the default) means that the operating system's default setting is used.

This parameter only has an effect if TCP keep-alive is enabled via the ``keepAlive`` parameter. The functionality may not be available on all platforms.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.keepalive-time-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive="on" keepAlive.time="60")

Input usage
-----------
.. _imtcp.parameter.input.keepalive-time-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" keepAlive="on" keepAlive.time="120")

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
