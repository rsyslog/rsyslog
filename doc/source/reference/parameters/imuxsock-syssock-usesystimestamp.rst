.. _param-imuxsock-syssock-usesystimestamp:
.. _imuxsock.parameter.module.syssock-usesystimestamp:

SysSock.UseSysTimeStamp
=======================

.. index::
   single: imuxsock; SysSock.UseSysTimeStamp
   single: SysSock.UseSysTimeStamp

.. summary-start

Obtains message time from the system instead of the message content.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.UseSysTimeStamp
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 5.9.1

Description
-----------
The same as the input parameter ``UseSysTimeStamp``, but for the system log
socket. This parameter instructs ``imuxsock`` to obtain message time from
the system (via control messages) instead of using time recorded inside
the message. This may be most useful in combination with systemd. Due to
the usefulness of this functionality, we decided to enable it by default.
As such, the behavior is slightly different than previous versions.
However, we do not see how this could negatively affect existing environments.

Module usage
------------
.. _param-imuxsock-module-syssock-usesystimestamp:
.. _imuxsock.parameter.module.syssock-usesystimestamp-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.useSysTimeStamp="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.systemlogusesystimestamp:

- $SystemLogUseSysTimeStamp — maps to SysSock.UseSysTimeStamp (status: legacy)

.. index::
   single: imuxsock; $SystemLogUseSysTimeStamp
   single: $SystemLogUseSysTimeStamp

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
