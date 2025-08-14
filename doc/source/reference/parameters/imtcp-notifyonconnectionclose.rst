.. _param-imtcp-notifyonconnectionclose:
.. _imtcp.parameter.module.notifyonconnectionclose:
.. _imtcp.parameter.input.notifyonconnectionclose:

NotifyOnConnectionClose
=======================

.. index::
   single: imtcp; NotifyOnConnectionClose
   single: NotifyOnConnectionClose

.. summary-start

Controls whether a syslog message is generated when a remote peer closes a connection.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: NotifyOnConnectionClose
:Scope: module, input
:Type: boolean
:Default: ``off`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
If enabled, this instructs ``imtcp`` to emit a syslog message each time a remote peer closes a connection to a TCP listener.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.notifyonconnectionclose-usage:

.. code-block:: rsyslog

   module(load="imtcp" notifyOnConnectionClose="on")

Input usage
-----------
.. _imtcp.parameter.input.notifyonconnectionclose-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" notifyOnConnectionClose="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpservernotifyonconnectionclose:
- $InputTCPServerNotifyOnConnectionClose — maps to NotifyOnConnectionClose (status: legacy)

.. index::
   single: imtcp; $InputTCPServerNotifyOnConnectionClose
   single: $InputTCPServerNotifyOnConnectionClose

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
