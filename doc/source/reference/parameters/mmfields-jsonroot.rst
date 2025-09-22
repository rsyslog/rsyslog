.. _param-mmfields-jsonroot:
.. _mmfields.parameter.input.jsonroot:

jsonroot
========

.. index::
   single: mmfields; jsonroot
   single: jsonroot

.. summary-start

Sets the JSON path that receives the extracted fields.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmfields`.

:Name: jsonroot
:Scope: input
:Type: string
:Default: !
:Required?: no
:Introduced: 7.5.1

Description
-----------
This parameter specifies into which JSON path the extracted fields shall be
written. The default is to use the JSON root object itself.

Input usage
-----------
.. _param-mmfields-input-jsonroot-usage:
.. _mmfields.parameter.input.jsonroot-usage:

.. code-block:: rsyslog

   module(load="mmfields")
   action(type="mmfields" jsonRoot="!mmfields")

See also
--------
See also :doc:`../../configuration/modules/mmfields`.
