.. _param-imuxsock-syssock-ignoretimestamp:
.. _imuxsock.parameter.module.syssock-ignoretimestamp:

SysSock.IgnoreTimestamp
=======================

.. index::
   single: imuxsock; SysSock.IgnoreTimestamp
   single: SysSock.IgnoreTimestamp

.. summary-start

Ignores timestamps included in messages from the system log socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.IgnoreTimestamp
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Ignore timestamps included in the messages, applies to messages received via
the system log socket.

Module usage
------------
.. _param-imuxsock-module-syssock-ignoretimestamp:
.. _imuxsock.parameter.module.syssock-ignoretimestamp-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.ignoreTimestamp="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.systemlogsocketignoremsgtimestamp:

- $SystemLogSocketIgnoreMsgTimestamp — maps to SysSock.IgnoreTimestamp (status: legacy)

.. index::
   single: imuxsock; $SystemLogSocketIgnoreMsgTimestamp
   single: $SystemLogSocketIgnoreMsgTimestamp

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
