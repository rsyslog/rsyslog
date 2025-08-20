.. _param-omprog-closetimeout:
.. _omprog.parameter.action.closetimeout:

closeTimeout
============

.. index::
   single: omprog; closeTimeout
   single: closeTimeout

.. summary-start

Waits the specified milliseconds for the program to exit after stdin closes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: closeTimeout
:Scope: action
:Type: integer
:Default: action=5000
:Required?: no
:Introduced: 8.35.0

Description
-----------
Specifies how long rsyslog must wait for the external program to terminate
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown) after closing the pipe,
that is, after sending EOF to the stdin of the child process. The value must
be expressed in milliseconds and must be greater than or equal to zero.

See the :ref:`param-omprog-killunresponsive` parameter for more details.

Action usage
------------
.. _param-omprog-action-closetimeout:
.. _omprog.parameter.action.closetimeout-usage:

.. code-block:: rsyslog

   action(type="omprog" closeTimeout="10000")

See also
--------
See also :doc:`../../configuration/modules/omprog`.
