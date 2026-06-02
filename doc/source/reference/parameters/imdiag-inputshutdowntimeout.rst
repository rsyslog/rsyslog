.. _param-imdiag-inputshutdowntimeout:
.. _imdiag.parameter.module.inputshutdowntimeout:

.. meta::
   :description: Reference for the imdiag inputShutdownTimeout module parameter.
   :keywords: rsyslog, imdiag, inputshutdowntimeout, testbench, timeout, shutdown

InputShutdownTimeout
====================

.. index::
   single: imdiag; InputShutdownTimeout

.. summary-start

Sets the input shutdown timeout at config load time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: InputShutdownTimeout
:Scope: module
:Type: integer (milliseconds)
:Default: 60000
:Required?: no
:Introduced: 8.x

Description
-----------
Sets ``globals.inputTimeoutShutdown`` — the time rsyslog allows each input
plugin to stop during an orderly shutdown — at config-load time. The
testbench uses this to ensure inputs have enough time to shut down cleanly
under load, in both RainerScript and YAML-only modes.

Override per-test via ``RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT`` before
``generate_conf``.

Module usage
------------

.. code-block:: rsyslog

   module(load="imdiag" inputShutdownTimeout="60000")

YAML usage
----------

.. code-block:: yaml

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       inputshutdowntimeout: "60000"

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
