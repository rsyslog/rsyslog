.. _param-imtcp-addtlframedelimiter:
.. _imtcp.parameter.module.addtlframedelimiter:
.. _imtcp.parameter.input.addtlframedelimiter:

AddtlFrameDelimiter
===================

.. index::
   single: imtcp; AddtlFrameDelimiter
   single: AddtlFrameDelimiter

.. summary-start

Specifies an additional frame delimiter for message reception.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: AddtlFrameDelimiter
:Scope: module, input
:Type: integer
:Default: module=-1, input=module parameter
:Required?: no
:Introduced: 4.3.1

Description
-----------
This directive permits to specify an additional frame delimiter. The value
must be the decimal ASCII code of the character to be used as the delimiter.
For example, to use ``#`` as the delimiter, specify ``addtlFrameDelimiter="35"``.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-addtlframedelimiter:
.. _imtcp.parameter.module.addtlframedelimiter-usage:

.. code-block:: rsyslog

   module(load="imtcp" addtlFrameDelimiter="...")

Input usage
-----------
.. _param-imtcp-input-addtlframedelimiter:
.. _imtcp.parameter.input.addtlframedelimiter-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" addtlFrameDelimiter="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserveraddtlframedelimiter:

- $InputTCPServerAddtlFrameDelimiter — maps to AddtlFrameDelimiter (status: legacy)

.. index::
   single: imtcp; $InputTCPServerAddtlFrameDelimiter
   single: $InputTCPServerAddtlFrameDelimiter

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

