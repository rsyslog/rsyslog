********************************************************
Support module for external message modification modules
********************************************************

===========================  ===========================================================================
**Module Name:**             **mmexternal**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         8.3.0
===========================  ===========================================================================


Purpose
=======

This module permits to integrate external message modification plugins
into rsyslog.

For details on the interface specification, see rsyslog's source in the
./plugins/external/INTERFACE.md.
 

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmexternal-binary`
     - .. include:: ../../reference/parameters/mmexternal-binary.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmexternal-interface-input`
     - .. include:: ../../reference/parameters/mmexternal-interface-input.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmexternal-output`
     - .. include:: ../../reference/parameters/mmexternal-output.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmexternal-forcesingleinstance`
     - .. include:: ../../reference/parameters/mmexternal-forcesingleinstance.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmexternal-binary
   ../../reference/parameters/mmexternal-interface-input
   ../../reference/parameters/mmexternal-output
   ../../reference/parameters/mmexternal-forcesingleinstance


Examples
========

Execute external module
-----------------------

The following config file snippet is used to write execute an external
message modification module "mmexternal.py". Note that the path to the
module is specified here. This is necessary if the module is not in the
default search path.

.. code-block:: none

   module (load="mmexternal") # needs to be done only once inside the config

   action(type="mmexternal" binary="/path/to/mmexternal.py")


