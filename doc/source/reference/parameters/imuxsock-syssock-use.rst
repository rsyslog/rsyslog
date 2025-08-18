.. _param-imuxsock-syssock-use:
.. _imuxsock.parameter.module.syssock-use:

SysSock.Use
===========

.. index::
   single: imuxsock; SysSock.Use
   single: SysSock.Use

.. summary-start

Enables listening on the system log socket or the path set by SysSock.Name.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.Use
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Listen on the default local log socket (``/dev/log``) or, if provided, use
the log socket value assigned to the ``SysSock.Name`` parameter instead
of the default. This is most useful if you run multiple instances of
rsyslogd where only one shall handle the system log socket. Unless
disabled by the ``SysSock.Unlink`` setting, this socket is created
upon rsyslog startup and deleted upon shutdown, according to
traditional syslogd behavior.

The behavior of this parameter is different for systemd systems. For those
systems, ``SysSock.Use`` still needs to be enabled, but the value of
``SysSock.Name`` is ignored and the socket provided by systemd is used
instead. If this parameter is *not* enabled, then imuxsock will only be
of use if a custom input is configured.

See the :ref:`imuxsock-systemd-details-label` section for details.

Module usage
------------
.. _param-imuxsock-module-syssock-use:
.. _imuxsock.parameter.module.syssock-use-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.use="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.omitlocallogging:

- $OmitLocalLogging — maps to SysSock.Use (status: legacy)

.. index::
   single: imuxsock; $OmitLocalLogging
   single: $OmitLocalLogging

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
