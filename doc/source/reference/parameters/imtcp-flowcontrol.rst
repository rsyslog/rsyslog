.. _param-imtcp-flowcontrol:
.. _imtcp.parameter.module.flowcontrol:
.. _imtcp.parameter.input.flowcontrol:

FlowControl
===========

.. index::
   single: imtcp; FlowControl
   single: FlowControl

.. summary-start

Enables or disables light-delayable message flow control for TCP inputs.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: FlowControl
:Scope: module, input
:Type: boolean
:Default: ``on`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This setting specifies whether some message flow control shall be exercised on the related TCP input. If set to on, messages are handled as "light delayable," which means the sender is throttled slightly when the main queue becomes near-full. This is done to preserve queue space for inputs that cannot be throttled (like UDP). While this is the default and generally useful, it may have undesired effects in some configurations. To disable this behavior, explicitly set the parameter to "off".

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.flowcontrol-usage:

.. code-block:: rsyslog

   module(load="imtcp" flowControl="off")

Input usage
-----------
.. _imtcp.parameter.input.flowcontrol-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" flowControl="off")

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
