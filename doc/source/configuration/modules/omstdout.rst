***********************************************
omstdout: stdout output module (testbench tool)
***********************************************

===========================  ===========================================================================
**Module Name:**             **omstdout**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available Since:**         4.1.6
===========================  ===========================================================================


Purpose
=======

This module writes any messages that are passed to it to stdout. It
was developed for the rsyslog test suite. However, there may (limited)
exist some other usages. Please note we do not put too much effort on
the quality of this module as we do not expect it to be used in real
deployments. If you do, please drop us a note so that we can enhance
its priority!


Configuration
=============

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - *(none)*
     - This module has no module-scope parameters.


Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omstdout-template`
     - .. include:: ../../reference/parameters/omstdout-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omstdout-ensurelfending`
     - .. include:: ../../reference/parameters/omstdout-ensurelfending.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omstdout-template
   ../../reference/parameters/omstdout-ensurelfending


Configure statement
-------------------

This is used when building rsyslog from source.

./configure --enable-omstdout


Legacy parameter not adopted in the new style
---------------------------------------------

-  **$ActionOMStdoutArrayInterface**
   [Default: off]
   This setting instructs omstdout to use the alternate array based
   method of parameter passing. If used, the values will be output with
   commas between the values but no other padding bytes. This is a test
   aid for the alternate calling interface.


Examples
========

Minimum setup
-------------

The following sample is the minimum setup required to have syslog messages
written to stdout.

.. code-block:: none

   module(load="omstdout")
   action(type="omstdout")


Example 2
---------

The following sample will write syslog messages to stdout, using a template.

.. code-block:: none

   module(load="omstdout")
   action(type="omstdout" template="outfmt")


