.. _param-mmfields-jsonroot:
.. _mmfields.parameter.module.jsonroot:

jsonRoot
========

.. index::
   single: mmfields; jsonRoot
   single: jsonRoot

.. summary-start

Sets the JSON path that receives the extracted fields.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmfields`.

:Name: jsonRoot
:Scope: module
:Type: string
:Default: !
:Required?: no
:Introduced: 7.5.1

Description
-----------
This parameter specifies into which JSON path the extracted fields shall be
written. The default is to use the JSON root object itself.

Module usage
------------
.. _param-mmfields-module-jsonroot-usage:
.. _mmfields.parameter.module.jsonroot-usage:

.. code-block:: rsyslog

   module(load="mmfields")
   action(type="mmfields" jsonRoot="!mmfields")

See also
--------
See also :doc:`../../configuration/modules/mmfields`.
