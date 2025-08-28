.. _param-omprog-signalonclose:
.. _omprog.parameter.action.signalonclose:

signalOnClose
=============

.. index::
   single: omprog; signalOnClose
   single: signalOnClose

.. summary-start

Sends a TERM signal to the program before closing its stdin.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: signalOnClose
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: 8.23.0

Description
-----------
Specifies whether a TERM signal must be sent to the external program before
closing it (when either the worker thread has been unscheduled, a restart
of the program is being forced, or rsyslog is about to shutdown).

If this switch is set to "on", rsyslog will send a TERM signal to the child
process before closing the pipe. That is, the process will first receive a
TERM signal, and then an EOF on stdin.

No signal is issued if this switch is set to "off" (default). The child
process can still detect it must terminate because reading from stdin will
return EOF.

See the :ref:`param-omprog-killunresponsive` parameter for more details.

Action usage
------------
.. _param-omprog-action-signalonclose:
.. _omprog.parameter.action.signalonclose-usage:

.. code-block:: rsyslog

   action(type="omprog" signalOnClose="on")

Notes
-----
- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------
See also :doc:`../../configuration/modules/omprog`.
