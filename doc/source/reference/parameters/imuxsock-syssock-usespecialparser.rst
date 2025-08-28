.. _param-imuxsock-syssock-usespecialparser:
.. _imuxsock.parameter.module.syssock-usespecialparser:

SysSock.UseSpecialParser
========================

.. index::
   single: imuxsock; SysSock.UseSpecialParser
   single: SysSock.UseSpecialParser

.. summary-start

Uses a specialized parser for the typical system log socket format.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.UseSpecialParser
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 8.9.0

Description
-----------
The equivalent of the ``UseSpecialParser`` input parameter, but
for the system socket. If turned on (the default) a special parser is
used that parses the format that is usually used
on the system log socket (the one :manpage:`syslog(3)` creates). If set to
"off", the regular parser chain is used, in which case the format on the
log socket can be arbitrary.

.. note::

   When the special parser is used, rsyslog is able to inject a more precise
   timestamp into the message (it is obtained from the log socket). If the
   regular parser chain is used, this is not possible.

Module usage
------------
.. _param-imuxsock-module-syssock-usespecialparser:
.. _imuxsock.parameter.module.syssock-usespecialparser-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.useSpecialParser="off")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
