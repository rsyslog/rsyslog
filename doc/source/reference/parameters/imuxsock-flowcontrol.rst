.. _param-imuxsock-flowcontrol:
.. _imuxsock.parameter.input.flowcontrol:

FlowControl
===========

.. index::
   single: imuxsock; FlowControl
   single: FlowControl

.. summary-start

Enables flow control on this input's socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: FlowControl
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies if flow control should be applied to the input being defined.

Input usage
-----------
.. _param-imuxsock-input-flowcontrol:
.. _imuxsock.parameter.input.flowcontrol-usage:

.. code-block:: rsyslog

   input(type="imuxsock" flowControl="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.inputunixlistensocketflowcontrol:
- $InputUnixListenSocketFlowControl — maps to FlowControl (status: legacy)

.. index::
   single: imuxsock; $InputUnixListenSocketFlowControl
   single: $InputUnixListenSocketFlowControl

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
