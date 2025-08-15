.. _param-imtcp-flowcontrol:
.. _imtcp.parameter.module.flowcontrol:

FlowControl
===========

.. index::
   single: imtcp; FlowControl
   single: FlowControl

.. summary-start

Applies light flow control to throttle senders when queues near full.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: FlowControl
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This setting specifies whether some message flow control shall be
exercised on the related TCP input. If set to on, messages are
handled as "light delayable", which means the sender is throttled a
bit when the queue becomes near-full. This is done in order to
preserve some queue space for inputs that can not throttle (like
UDP), but it may have some undesired effect in some configurations.
Still, we consider this as a useful setting and thus it is the
default. To turn the handling off, simply configure that explicitly.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-flowcontrol:
.. _imtcp.parameter.module.flowcontrol-usage:

.. code-block:: rsyslog

   module(load="imtcp" flowControl="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpflowcontrol:

- $InputTCPFlowControl — maps to FlowControl (status: legacy)

.. index::
   single: imtcp; $InputTCPFlowControl
   single: $InputTCPFlowControl

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

