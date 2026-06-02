.. _param-imdiag-mainmsgqueuetimeoutenqueue:
.. _imdiag.parameter.module.mainmsgqueuetimeoutenqueue:

.. meta::
   :description: Reference for the imdiag mainMsgQueueTimeoutEnqueue module parameter.
   :keywords: rsyslog, imdiag, mainmsgqueuetimeoutenqueue, testbench, timeout, queue

MainMsgQueueTimeoutEnqueue
===========================

.. index::
   single: imdiag; MainMsgQueueTimeoutEnqueue

.. summary-start

Sets the main message queue enqueue timeout at config load time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: MainMsgQueueTimeoutEnqueue
:Scope: module
:Type: integer (milliseconds)
:Default: 30000
:Required?: no
:Introduced: 8.x

Description
-----------
Sets ``globals.mainQ.iMainMsgQtoEnq`` — the time a producer will wait when
the main message queue is full before dropping the message — at config-load
time. The testbench uses this to establish a safe default without relying on
legacy ``$MainMsgQueueTimeoutEnqueue`` directives, which are not available in
YAML-only mode.

Override per-test via ``RSTB_MAIN_Q_TO_ENQUEUE`` before
``generate_conf``, or via a ``$MainMsgQueueTimeoutEnqueue`` directive in the
RainerScript test fragment.

Module usage
------------

.. code-block:: rsyslog

   module(load="imdiag" mainMsgQueueTimeoutEnqueue="30000")

YAML usage
----------

.. code-block:: yaml

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       mainmsgqueuetimeoutenqueue: "30000"

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
