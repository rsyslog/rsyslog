***********************************
imdiag: Diagnostic instrumentation
***********************************

===========================  ===============================
**Module Name:**             **imdiag**
**Author:**                  Rainer Gerhards
**Available since:**         at least 5.x
===========================  ===============================

Purpose
=======

The imdiag input module exposes a TCP-based diagnostics and control channel
that can inject messages into the
main queue, wait for queues to drain, coordinate statistics reporting, and
exercise other helper functions used by the rsyslog testbench. While imdiag is
primarily intended for automated testing, it can also be used to diagnose
production systems. Because the interface permits queue control and message
injection, it **must only be exposed to trusted hosts**.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for
   readability.

Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imdiag-aborttimeout`
     - .. include:: ../../reference/parameters/imdiag-aborttimeout.rst
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
----------------

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

.. toctree::
   :hidden:

   ../../reference/parameters/imdiag-aborttimeout
   ../../reference/parameters/imdiag-injectdelaymode
   ../../reference/parameters/imdiag-maxsessions
   ../../reference/parameters/imdiag-listenportfilename
   ../../reference/parameters/imdiag-serverrun
   ../../reference/parameters/imdiag-serverstreamdrivermode
   ../../reference/parameters/imdiag-serverstreamdriverauthmode
   ../../reference/parameters/imdiag-serverstreamdriverpermittedpeer
   ../../reference/parameters/imdiag-serverinputname
