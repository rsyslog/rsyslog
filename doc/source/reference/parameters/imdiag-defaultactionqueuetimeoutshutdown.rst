.. _param-imdiag-defaultactionqueuetimeoutshutdown:
.. _imdiag.parameter.module.defaultactionqueuetimeoutshutdown:

.. meta::
   :description: Reference for the imdiag defaultActionQueueTimeoutShutdown module parameter.
   :keywords: rsyslog, imdiag, defaultactionqueuetimeoutshutdown, testbench, timeout, action queue

DefaultActionQueueTimeoutShutdown
==================================

.. index::
   single: imdiag; DefaultActionQueueTimeoutShutdown

.. summary-start

Sets the default action queue shutdown timeout at config load time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: DefaultActionQueueTimeoutShutdown
:Scope: module
:Type: integer (milliseconds)
:Default: 20000
:Required?: no
:Introduced: 8.x

Description
-----------
Sets ``globals.actq_dflt_toQShutdown`` — the default shutdown timeout for
action queues created after this point — at config-load time. This only
affects action queues that do not specify their own timeout. The testbench
uses this to ensure action queues drain cleanly under test conditions.

.. note::
   This parameter affects action queues created *after* the module is
   loaded. Existing queues are unaffected.

Override per-test via ``RSTB_ACTION_DEFAULT_Q_TO_SHUTDOWN`` before
``generate_conf``.

Module usage
------------

.. code-block:: rsyslog

   module(load="imdiag" defaultActionQueueTimeoutShutdown="20000")

YAML usage
----------

.. code-block:: yaml

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       defaultactionqueuetimeoutshutdown: "20000"

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
