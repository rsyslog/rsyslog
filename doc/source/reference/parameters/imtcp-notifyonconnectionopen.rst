.. _param-imtcp-notifyonconnectionopen:
.. _imtcp.parameter.module.notifyonconnectionopen:

NotifyOnConnectionOpen
======================

.. index::
   single: imtcp; NotifyOnConnectionOpen
   single: NotifyOnConnectionOpen

.. summary-start

Emits a message when a remote peer opens a connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: NotifyOnConnectionOpen
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Instructs imtcp to emit a message if the remote peer opens a
connection.

Module usage
------------
.. _param-imtcp-module-notifyonconnectionopen:
.. _imtcp.parameter.module.notifyonconnectionopen-usage:

.. code-block:: rsyslog

   module(load="imtcp" notifyOnConnectionOpen="on")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

