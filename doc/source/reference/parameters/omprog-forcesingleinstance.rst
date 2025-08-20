.. _param-omprog-forcesingleinstance:
.. _omprog.parameter.action.forcesingleinstance:

forceSingleInstance
===================

.. index::
   single: omprog; forceSingleInstance
   single: forceSingleInstance

.. summary-start

Runs only one instance of the program regardless of worker threads.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: forceSingleInstance
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: v8.1.6

Description
-----------
By default, the omprog action will start an instance (process) of the
external program per worker thread (the maximum number of worker threads
can be specified with the :doc:`queue.workerThreads <../../rainerscript/queue_parameters>`
parameter). Moreover, if the action is associated to a
:doc:`disk-assisted queue <../../concepts/queues>`, an additional instance
will be started when the queue is persisted, to process the items stored
on disk.

If you want to force a single instance of the program to be executed,
regardless of the number of worker threads or the queue type, set this
flag to "on". This is useful when the external program uses or accesses
some kind of shared resource that does not allow concurrent access from
multiple processes.

.. note::

   Before version v8.38.0, this parameter had no effect.

Action usage
------------
.. _param-omprog-action-forcesingleinstance:
.. _omprog.parameter.action.forcesingleinstance-usage:

.. code-block:: rsyslog

   action(type="omprog" forceSingleInstance="on")

Notes
-----
- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------
See also :doc:`../../configuration/modules/omprog`.
