imdiag: Diagnostic instrumentation
==================================

:Module Name: **imdiag**
:Author: Rainer Gerhards
:Available since: at least 5.x

Purpose
-------

The imdiag input module exposes a TCP-based diagnostics and control channel
that can inject messages into the
main queue, wait for queues to drain, coordinate statistics reporting, and
exercise other helper functions used by the rsyslog testbench. While imdiag is
primarily intended for automated testing, it can also be used to diagnose
production systems. Because the interface permits queue control and message
injection, it **must only be exposed to trusted hosts**.

Configuration Parameters
------------------------

.. note::

   Parameter names are case-insensitive; camelCase is recommended for
   readability.

Module Parameters
~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imdiag-aborttimeout`
     - .. include:: ../../reference/parameters/imdiag-aborttimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-listenportfilename-module`
     - .. include:: ../../reference/parameters/imdiag-listenportfilename-module.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-mainmsgqueuetimeoutshutdown`
     - .. include:: ../../reference/parameters/imdiag-mainmsgqueuetimeoutshutdown.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-mainmsgqueuetimeoutenqueue`
     - .. include:: ../../reference/parameters/imdiag-mainmsgqueuetimeoutenqueue.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-inputshutdowntimeout`
     - .. include:: ../../reference/parameters/imdiag-inputshutdowntimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-defaultactionqueuetimeoutshutdown`
     - .. include:: ../../reference/parameters/imdiag-defaultactionqueuetimeoutshutdown.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-defaultactionqueuetimeoutenqueue`
     - .. include:: ../../reference/parameters/imdiag-defaultactionqueuetimeoutenqueue.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-injectdelaymode`
     - .. include:: ../../reference/parameters/imdiag-injectdelaymode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-maxsessions`
     - .. include:: ../../reference/parameters/imdiag-maxsessions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Input Parameters
~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imdiag-listenportfilename`
     - .. include:: ../../reference/parameters/imdiag-listenportfilename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-serverrun`
     - .. include:: ../../reference/parameters/imdiag-serverrun.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-serverstreamdrivermode`
     - .. include:: ../../reference/parameters/imdiag-serverstreamdrivermode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-serverstreamdriverauthmode`
     - .. include:: ../../reference/parameters/imdiag-serverstreamdriverauthmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-serverstreamdriverpermittedpeer`
     - .. include:: ../../reference/parameters/imdiag-serverstreamdriverpermittedpeer.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdiag-serverinputname`
     - .. include:: ../../reference/parameters/imdiag-serverinputname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Examples
--------

Minimal configuration for testbench integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This example loads ``imdiag``, starts the diagnostic listener on an
ephemeral port, and records the chosen port for the testbench to read.

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag"
         listenPortFileName="/var/run/rsyslog/imdiag.port"
         serverRun="0")

YAML-only testbench configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In YAML-only mode the testbench preamble uses the ``testbench_modules:``
key (an alias for ``modules:`` reserved for testbench infrastructure) so
that it does not conflict with the test's own ``modules:`` section.

.. code-block:: yaml

   version: 2

   global:
     debug.abortOnProgramError: "on"

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       listenportfilename: "test.imdiag.port"
       aborttimeout: "580"
       mainmsgqueuetimeoutshutdown: "10000"
       mainmsgqueuetimeoutenqueue: "30000"
       inputshutdowntimeout: "60000"
       defaultactionqueuetimeoutshutdown: "20000"
       defaultactionqueuetimeoutenqueue: "30000"

   modules:
     - load: "../plugins/imtcp/.libs/imtcp"

   inputs:
     - type: imdiag
       port: "0"
     - type: imtcp
       port: "0"

.. toctree::
   :hidden:

   ../../reference/parameters/imdiag-aborttimeout
   ../../reference/parameters/imdiag-listenportfilename-module
   ../../reference/parameters/imdiag-mainmsgqueuetimeoutshutdown
   ../../reference/parameters/imdiag-mainmsgqueuetimeoutenqueue
   ../../reference/parameters/imdiag-inputshutdowntimeout
   ../../reference/parameters/imdiag-defaultactionqueuetimeoutshutdown
   ../../reference/parameters/imdiag-defaultactionqueuetimeoutenqueue
   ../../reference/parameters/imdiag-injectdelaymode
   ../../reference/parameters/imdiag-maxsessions
   ../../reference/parameters/imdiag-listenportfilename
   ../../reference/parameters/imdiag-serverrun
   ../../reference/parameters/imdiag-serverstreamdrivermode
   ../../reference/parameters/imdiag-serverstreamdriverauthmode
   ../../reference/parameters/imdiag-serverstreamdriverpermittedpeer
   ../../reference/parameters/imdiag-serverinputname
