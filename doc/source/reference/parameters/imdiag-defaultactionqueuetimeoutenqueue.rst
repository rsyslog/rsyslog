.. _param-imdiag-defaultactionqueuetimeoutenqueue:
.. _imdiag.parameter.module.defaultactionqueuetimeoutenqueue:

.. meta::
   :description: Reference for the imdiag defaultActionQueueTimeoutEnqueue module parameter.
   :keywords: rsyslog, imdiag, defaultactionqueuetimeoutenqueue, testbench, timeout, action queue

DefaultActionQueueTimeoutEnqueue
=================================

.. index::
   single: imdiag; DefaultActionQueueTimeoutEnqueue

.. summary-start

Sets the default action queue enqueue timeout at config load time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: DefaultActionQueueTimeoutEnqueue
:Scope: module
:Type: integer (milliseconds)
:Default: 30000
:Required?: no
:Introduced: 8.x

Description
-----------
Sets ``globals.actq_dflt_toEnq`` — the default enqueue timeout for action
queues created after this point — at config-load time. This only affects
action queues that do not specify their own timeout. When an action queue is
full, a producer waits up to this duration before dropping the message.

.. note::
   This parameter affects action queues created *after* the module is
   loaded. Existing queues are unaffected.

Override per-test via ``RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE`` before
``generate_conf``.

Module usage
------------

.. code-block:: rsyslog

   module(load="imdiag" defaultActionQueueTimeoutEnqueue="30000")

YAML usage
----------

.. code-block:: yaml

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       defaultactionqueuetimeoutenqueue: "30000"

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
