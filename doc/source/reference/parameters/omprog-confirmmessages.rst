.. _param-omprog-confirmmessages:
.. _omprog.parameter.action.confirmmessages:

confirmMessages
===============

.. index::
   single: omprog; confirmMessages
   single: confirmMessages

.. summary-start

Waits for the program to acknowledge each message via stdout.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: confirmMessages
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: 8.31.0

Description
-----------
Specifies whether the external program provides feedback to rsyslog via stdout.
When this switch is set to "on", rsyslog will wait for the program to confirm
each received message. This feature facilitates error handling: instead of
having to implement a retry logic, the external program can rely on the rsyslog
queueing capabilities.

To confirm a message, the program must write a line with the word ``OK`` to its
standard output. If it writes a line containing anything else, rsyslog considers
that the message could not be processed, keeps it in the action queue, and
re-sends it to the program later (after the period specified by the
:doc:`action.resumeInterval <../../configuration/actions>` parameter).

In addition, when a new instance of the program is started, rsyslog will also
wait for the program to confirm it is ready to start consuming logs. This
prevents rsyslog from starting to send logs to a program that could not
complete its initialization properly.

.. seealso::

   `Interface between rsyslog and external output plugins
   <https://github.com/rsyslog/rsyslog/blob/master/plugins/external/INTERFACE.md>`_

Action usage
------------
.. _param-omprog-action-confirmmessages:
.. _omprog.parameter.action.confirmmessages-usage:

.. code-block:: rsyslog

   action(type="omprog" confirmMessages="on")

Notes
-----
- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------
See also :doc:`../../configuration/modules/omprog`.
