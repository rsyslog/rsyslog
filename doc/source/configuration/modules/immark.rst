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

.. toctree::
   :hidden:

   ../../reference/parameters/immark-interval
