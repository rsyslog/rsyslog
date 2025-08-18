.. _param-imuxsock-syssock-usepidfromsystem:
.. _imuxsock.parameter.module.syssock-usepidfromsystem:

SysSock.UsePIDFromSystem
========================

.. index::
   single: imuxsock; SysSock.UsePIDFromSystem
   single: SysSock.UsePIDFromSystem

.. summary-start

Obtains the logged PID from the log socket itself.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.UsePIDFromSystem
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 5.7.0

Description
-----------
Specifies if the pid being logged shall be obtained from the log socket
itself. If so, the TAG part of the message is rewritten. It is recommended
to turn this option on, but the default is "off" to keep compatible
with earlier versions of rsyslog.

Module usage
------------
.. _param-imuxsock-module-syssock-usepidfromsystem:
.. _imuxsock.parameter.module.syssock-usepidfromsystem-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.usePIDFromSystem="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.systemlogusepidfromsystem:

- $SystemLogUsePIDFromSystem — maps to SysSock.UsePIDFromSystem (status: legacy)

.. index::
   single: imuxsock; $SystemLogUsePIDFromSystem
   single: $SystemLogUsePIDFromSystem

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
