**********************************
immark: Mark Message Input Module
**********************************

===========================  ===========================================================================
**Module Name:**             **immark**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

Purpose
=======

This module provides the ability to inject periodic "mark" messages to
the input of rsyslog. This is useful to allow for verification that
the logging system is functioning.

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
   * - :ref:`param-immark-interval`
     - .. include:: ../../reference/parameters/immark-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-immark-markmessagetext`
     - .. include:: ../../reference/parameters/immark-markmessagetext.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-immark-ruleset`
     - .. include:: ../../reference/parameters/immark-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-immark-use-markflag`
     - .. include:: ../../reference/parameters/immark-use-markflag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-immark-use-syslogcall`
     - .. include:: ../../reference/parameters/immark-use-syslogcall.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/immark-interval
   ../../reference/parameters/immark-markmessagetext
   ../../reference/parameters/immark-ruleset
   ../../reference/parameters/immark-use-markflag
   ../../reference/parameters/immark-use-syslogcall
