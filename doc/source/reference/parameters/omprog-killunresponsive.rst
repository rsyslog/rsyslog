.. _param-omprog-killunresponsive:
.. _omprog.parameter.action.killunresponsive:

killUnresponsive
================

.. index::
   single: omprog; killUnresponsive
   single: killUnresponsive

.. summary-start

Kills the program if it remains after the close timeout expires.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: killUnresponsive
:Scope: action
:Type: boolean
:Default: action=the value of 'signalOnClose'
:Required?: no
:Introduced: 8.35.0

Description
-----------
Specifies whether a KILL signal must be sent to the external program in case
it does not terminate within the timeout indicated by :ref:`param-omprog-closetimeout`
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown).

If :ref:`param-omprog-signalonclose` is set to "on", the default value of ``killUnresponsive``
is also "on". In this case, the cleanup sequence of the child process is as
follows: (1) a TERM signal is sent to the child, (2) the pipe with the child
process is closed (the child will receive EOF on stdin), (3) rsyslog waits
for the child process to terminate during :ref:`param-omprog-closetimeout`,
(4) if the child has not terminated within the timeout, a KILL signal is sent to it.

If :ref:`param-omprog-signalonclose` is set to "off", the default value of ``killUnresponsive``
is also "off". In this case, the child cleanup sequence is as follows: (1) the
pipe with the child process is closed (the child will receive EOF on stdin),
(2) rsyslog waits for the child process to terminate during :ref:`param-omprog-closetimeout`,
(3) if the child has not terminated within the timeout, rsyslog ignores it.

This parameter can be set to a different value than :ref:`param-omprog-signalonclose`, obtaining
the corresponding variations of cleanup sequences described above.

Action usage
------------
.. _param-omprog-action-killunresponsive:
.. _omprog.parameter.action.killunresponsive-usage:

.. code-block:: rsyslog

   action(type="omprog" killUnresponsive="on")

Notes
-----
- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------
See also :doc:`../../configuration/modules/omprog`.
