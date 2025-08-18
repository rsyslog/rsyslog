.. _param-imuxsock-syssock-name:
.. _imuxsock.parameter.module.syssock-name:

SysSock.Name
============

.. index::
   single: imuxsock; SysSock.Name
   single: SysSock.Name

.. summary-start

Selects an alternate log socket instead of the default ``/dev/log``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.Name
:Scope: module
:Type: word
:Default: module=/dev/log
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies an alternate log socket to be used instead of the default system
log socket, traditionally ``/dev/log``. Unless disabled by the
``SysSock.Unlink`` setting, this socket is created upon rsyslog startup
and deleted upon shutdown, according to traditional syslogd behavior.

The behavior of this parameter is different for systemd systems. See the
:ref:`imuxsock-systemd-details-label` section for details.

Module usage
------------
.. _param-imuxsock-module-syssock-name:
.. _imuxsock.parameter.module.syssock-name-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.name="/var/run/rsyslog/devlog")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.systemlogsockname:

- $SystemLogSocketName — maps to SysSock.Name (status: legacy)

.. index::
   single: imuxsock; $SystemLogSocketName
   single: $SystemLogSocketName

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
