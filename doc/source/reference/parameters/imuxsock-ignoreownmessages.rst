.. _param-imuxsock-ignoreownmessages:
.. _imuxsock.parameter.input.ignoreownmessages:

IgnoreOwnMessages
=================

.. index::
   single: imuxsock; IgnoreOwnMessages
   single: IgnoreOwnMessages

.. summary-start

Suppresses messages that originated from the same rsyslog instance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: IgnoreOwnMessages
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 7.3.7

Description
-----------
Ignore messages that originated from the same instance of rsyslogd.
There usually is no reason to receive messages from ourselves. This
setting is vital when writing messages to the systemd journal.

.. seealso::

   See :doc:`omjournal <../../configuration/modules/omjournal>` module
   documentation for a more in-depth description.

Input usage
-----------
.. _param-imuxsock-input-ignoreownmessages:
.. _imuxsock.parameter.input.ignoreownmessages-usage:

.. code-block:: rsyslog

   input(type="imuxsock" ignoreOwnMessages="off")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
