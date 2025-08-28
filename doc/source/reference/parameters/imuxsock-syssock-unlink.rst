.. _param-imuxsock-syssock-unlink:
.. _imuxsock.parameter.module.syssock-unlink:

SysSock.Unlink
==============

.. index::
   single: imuxsock; SysSock.Unlink
   single: SysSock.Unlink

.. summary-start

Controls whether the system socket is unlinked and recreated on start and stop.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.Unlink
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 7.3.9

Description
-----------
If turned on (default), the system socket is unlinked and re-created
when opened and also unlinked when finally closed. Note that this
setting has no effect when running under systemd control (because
systemd handles the socket. See the :ref:`imuxsock-systemd-details-label`
section for details.

Module usage
------------
.. _param-imuxsock-module-syssock-unlink:
.. _imuxsock.parameter.module.syssock-unlink-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.unlink="off")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
