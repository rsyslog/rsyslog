.. _param-imptcp-notifyonconnectionopen:
.. _imptcp.parameter.input.notifyonconnectionopen:

NotifyOnConnectionOpen
======================

.. index::
   single: imptcp; NotifyOnConnectionOpen
   single: NotifyOnConnectionOpen

.. summary-start

Emits a message when a remote peer opens a connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: NotifyOnConnectionOpen
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Instructs imptcp to emit a message if a remote peer opens a
connection. Hostname of the remote peer is given in the message.

Input usage
-----------
.. _param-imptcp-input-notifyonconnectionopen:
.. _imptcp.parameter.input.notifyonconnectionopen-usage:

.. code-block:: rsyslog

   input(type="imptcp" notifyOnConnectionOpen="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
