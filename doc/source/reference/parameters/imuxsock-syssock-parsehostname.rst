.. _param-imuxsock-syssock-parsehostname:
.. _imuxsock.parameter.module.syssock-parsehostname:

SysSock.ParseHostname
=====================

.. index::
   single: imuxsock; SysSock.ParseHostname
   single: SysSock.ParseHostname

.. summary-start

Expects hostnames on the system log socket when special parsing is disabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.ParseHostname
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.9.0

Description
-----------
.. note::

   This option only has an effect if ``SysSock.UseSpecialParser`` is
   set to "off".

Normally, the local log sockets do *not* contain hostnames. If set
to on, parsers will expect hostnames just like in regular formats. If
set to off (the default), the parser chain is instructed to not expect
them.

Module usage
------------
.. _param-imuxsock-module-syssock-parsehostname:
.. _imuxsock.parameter.module.syssock-parsehostname-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.parseHostname="on")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
