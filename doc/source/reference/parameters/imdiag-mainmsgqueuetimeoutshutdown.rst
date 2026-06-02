.. _param-imdiag-mainmsgqueuetimeoutshutdown:
.. _imdiag.parameter.module.mainmsgqueuetimeoutshutdown:

.. meta::
   :description: Reference for the imdiag mainMsgQueueTimeoutShutdown module parameter.
   :keywords: rsyslog, imdiag, mainmsgqueuetimeoutshutdown, testbench, timeout, queue

MainMsgQueueTimeoutShutdown
============================

.. index::
   single: imdiag; MainMsgQueueTimeoutShutdown

.. summary-start

Sets the main message queue shutdown timeout at config load time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: MainMsgQueueTimeoutShutdown
:Scope: module
:Type: integer (milliseconds)
:Default: 10000
:Required?: no
:Introduced: 8.x

Description
-----------
Sets ``globals.mainQ.iMainMsgQtoQShutdown`` — the time rsyslog waits for
the main message queue to drain during an orderly shutdown — at config-load
time. The testbench uses this to establish a safe default without relying on
legacy ``$MainMsgQueueTimeoutShutdown`` directives, which are not available
in YAML-only mode.

The value can be overridden per-test by setting the
``RSTB_GLOBAL_QUEUE_SHUTDOWN_TIMEOUT`` environment variable before calling
``generate_conf``, or by writing a ``$MainMsgQueueTimeoutShutdown`` directive
in the test's RainerScript configuration fragment (legacy directives apply
after module params and therefore take precedence).

Module usage
------------

.. code-block:: rsyslog

   module(load="imdiag" mainMsgQueueTimeoutShutdown="10000")

YAML usage
----------

.. code-block:: yaml

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       mainmsgqueuetimeoutshutdown: "10000"

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
