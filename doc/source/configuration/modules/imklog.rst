*******************************
imklog: Kernel Log Input Module
*******************************

===========================  ===========================================================================
**Module Name:**             **imklog**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Reads messages from the kernel log and submits them to the syslog
engine.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imklog-internalmsgfacility`
     - .. include:: ../../reference/parameters/imklog-internalmsgfacility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imklog-permitnonkernelfacility`
     - .. include:: ../../reference/parameters/imklog-permitnonkernelfacility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imklog-consoleloglevel`
     - .. include:: ../../reference/parameters/imklog-consoleloglevel.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imklog-parsekerneltimestamp`
     - .. include:: ../../reference/parameters/imklog-parsekerneltimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imklog-keepkerneltimestamp`
     - .. include:: ../../reference/parameters/imklog-keepkerneltimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imklog-logpath`
     - .. include:: ../../reference/parameters/imklog-logpath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imklog-ratelimitinterval`
     - .. include:: ../../reference/parameters/imklog-ratelimitinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imklog-ratelimitburst`
     - .. include:: ../../reference/parameters/imklog-ratelimitburst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Caveats/Known Bugs
==================

This is obviously platform specific and requires platform drivers.
Currently, imklog functionality is available on Linux and BSD.

This module is **not supported on Solaris** and not needed there. For
Solaris kernel input, use :doc:`imsolaris <imsolaris>`.


Example 1
=========

The following sample pulls messages from the kernel log. All parameters
are left by default, which is usually a good idea. Please note that
loading the plugin is sufficient to activate it. No directive is needed
to start pulling kernel messages.

.. code-block:: none

   module(load="imklog")


Example 2
=========

The following sample adds a ratelimiter.  The burst and interval are
set high to allow for a large volume of messages on boot.

.. code-block:: none

  module(load="imklog" RatelimitBurst="5000" RatelimitInterval="5")


Unsupported |FmtObsoleteName| directives
========================================

.. function:: $DebugPrintKernelSymbols on/off

   Linux only, ignored on other platforms (but may be specified).
   Defaults to off.

.. function:: $klogLocalIPIF

   This directive is no longer supported. Instead, use the global
   $localHostIPIF directive instead.


.. function:: $klogUseSyscallInterface on/off

   Linux only, ignored on other platforms (but may be specified).
   Defaults to off.

.. function:: $klogSymbolsTwice on/off

   Linux only, ignored on other platforms (but may be specified).
   Defaults to off.


.. toctree::
   :hidden:

   ../../reference/parameters/imklog-internalmsgfacility
   ../../reference/parameters/imklog-permitnonkernelfacility
   ../../reference/parameters/imklog-consoleloglevel
   ../../reference/parameters/imklog-parsekerneltimestamp
   ../../reference/parameters/imklog-keepkerneltimestamp
   ../../reference/parameters/imklog-logpath
   ../../reference/parameters/imklog-ratelimitinterval
   ../../reference/parameters/imklog-ratelimitburst


