.. _param-imtcp-notifyonconnectionclose:
.. _imtcp.parameter.module.notifyonconnectionclose:
.. _imtcp.parameter.input.notifyonconnectionclose:

NotifyOnConnectionClose
=======================

.. index::
   single: imtcp; NotifyOnConnectionClose
   single: NotifyOnConnectionClose

.. summary-start

Emits a message when a remote peer closes a connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: NotifyOnConnectionClose
:Scope: module, input
:Type: boolean
:Default: module=off, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Instructs imtcp to emit a message if the remote peer closes a
connection.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-notifyonconnectionclose:
.. _imtcp.parameter.module.notifyonconnectionclose-usage:

.. code-block:: rsyslog

   module(load="imtcp" notifyOnConnectionClose="on")

Input usage
-----------
.. _param-imtcp-input-notifyonconnectionclose:
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

