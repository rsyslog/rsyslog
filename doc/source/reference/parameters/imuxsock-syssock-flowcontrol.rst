.. _param-imuxsock-syssock-flowcontrol:
.. _imuxsock.parameter.module.syssock-flowcontrol:

SysSock.FlowControl
===================

.. index::
   single: imuxsock; SysSock.FlowControl
   single: SysSock.FlowControl

.. summary-start

Applies flow control to the system log socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.FlowControl
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies if flow control should be applied to the system log socket.

Module usage
------------
.. _param-imuxsock-module-syssock-flowcontrol:
.. _imuxsock.parameter.module.syssock-flowcontrol-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.flowControl="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.systemlogflowcontrol:

- $SystemLogFlowControl — maps to SysSock.FlowControl (status: legacy)

.. index::
   single: imuxsock; $SystemLogFlowControl
   single: $SystemLogFlowControl

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
