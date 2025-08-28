.. _param-imuxsock-syssock-ignoreownmessages:
.. _imuxsock.parameter.module.syssock-ignoreownmessages:

SysSock.IgnoreOwnMessages
=========================

.. index::
   single: imuxsock; SysSock.IgnoreOwnMessages
   single: SysSock.IgnoreOwnMessages

.. summary-start

Ignores messages that originate from the same rsyslogd instance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.IgnoreOwnMessages
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 7.3.7

Description
-----------
Ignores messages that originated from the same instance of rsyslogd.
There usually is no reason to receive messages from ourselves.
This setting is vital when writing messages to the systemd journal.

.. seealso::

   See :doc:`omjournal <../../configuration/modules/omjournal>` module documentation for a more
   in-depth description.

Module usage
------------
.. _param-imuxsock-module-syssock-ignoreownmessages:
.. _imuxsock.parameter.module.syssock-ignoreownmessages-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.ignoreOwnMessages="off")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
