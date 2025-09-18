.. index:: ! mmdarwin

.. role:: json(code)
   :language: json

***************************
Darwin connector (mmdarwin)
***************************

================  ===========================================
**Module Name:**  **mmdarwin**
**Author:**       Guillaume Catto <guillaume.catto@advens.fr>,
                  Theo Bertin <theo.bertin@advens.fr>
================  ===========================================

Purpose
=======

Darwin is an open source Artificial Intelligence Framework for CyberSecurity. The mmdarwin module allows us to call Darwin in order to enrich our JSON-parsed logs with a score, and/or to allow Darwin to generate alerts.

How to build the module
=======================

To compile Rsyslog with mmdarwin you'll need to:

* set *--enable-mmdarwin* on configure

Configuration Parameter
=======================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

Module Parameters
-----------------
.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmdarwin-container`
     - .. include:: ../../reference/parameters/mmdarwin-container.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmdarwin-key`
     - .. include:: ../../reference/parameters/mmdarwin-key.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmdarwin-socketpath`
     - .. include:: ../../reference/parameters/mmdarwin-socketpath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmdarwin-response`
     - .. include:: ../../reference/parameters/mmdarwin-response.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmdarwin-filtercode`
     - .. include:: ../../reference/parameters/mmdarwin-filtercode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmdarwin-fields`
     - .. include:: ../../reference/parameters/mmdarwin-fields.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmdarwin-send_partial`
     - .. include:: ../../reference/parameters/mmdarwin-send_partial.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Configuration example
=====================

This example shows a possible configuration of mmdarwin.

.. code-block:: none

   module(load="imtcp")
   module(load="mmjsonparse")
   module(load="mmdarwin")

   input(type="imtcp" port="8042" Ruleset="darwin_ruleset")

   ruleset(name="darwin_ruleset") {
      action(type="mmjsonparse" cookie="")
      action(type="mmdarwin" socketpath="/path/to/reputation_1.sock" fields=["!srcip", "ATTACK;TOR"] key="reputation" response="back" filtercode="0x72657075")

      call darwin_output
   }

   ruleset(name="darwin_output") {
       action(type="omfile" file="/path/to/darwin_output.log")
   }

.. toctree::
   :hidden:

   ../../reference/parameters/mmdarwin-container
   ../../reference/parameters/mmdarwin-key
   ../../reference/parameters/mmdarwin-socketpath
   ../../reference/parameters/mmdarwin-response
   ../../reference/parameters/mmdarwin-filtercode
   ../../reference/parameters/mmdarwin-fields
   ../../reference/parameters/mmdarwin-send_partial
