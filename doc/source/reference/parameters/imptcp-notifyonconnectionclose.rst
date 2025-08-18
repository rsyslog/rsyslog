.. _param-imptcp-notifyonconnectionclose:
.. _imptcp.parameter.input.notifyonconnectionclose:

NotifyOnConnectionClose
=======================

.. index::
   single: imptcp; NotifyOnConnectionClose
   single: NotifyOnConnectionClose

.. summary-start

Emits a message when a remote peer closes the connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: NotifyOnConnectionClose
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Instructs imptcp to emit a message if a remote peer closes the
connection.

Input usage
-----------
.. _param-imptcp-input-notifyonconnectionclose:
.. _imptcp.parameter.input.notifyonconnectionclose-usage:

.. code-block:: rsyslog

   input(type="imptcp" notifyOnConnectionClose="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpservernotifyonconnectionclose:

- $InputPTCPServerNotifyOnConnectionClose — maps to NotifyOnConnectionClose (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerNotifyOnConnectionClose
   single: $InputPTCPServerNotifyOnConnectionClose

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
