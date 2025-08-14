.. _param-imtcp-notifyonconnectionopen:
.. _imtcp.parameter.module.notifyonconnectionopen:

NotifyOnConnectionOpen
======================

.. index::
   single: imtcp; NotifyOnConnectionOpen
   single: NotifyOnConnectionOpen

.. summary-start

Controls whether a syslog message is generated when a remote peer opens a connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: NotifyOnConnectionOpen
:Scope: module
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If enabled, this instructs ``imtcp`` to emit a syslog message each time a remote peer opens a connection to a TCP listener.

Module usage
------------
.. _imtcp.parameter.module.notifyonconnectionopen-usage:

.. code-block:: rsyslog

   module(load="imtcp" notifyOnConnectionOpen="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
