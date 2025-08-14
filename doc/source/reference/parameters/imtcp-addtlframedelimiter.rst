.. _param-imtcp-addtlframedelimiter:
.. _imtcp.parameter.module.addtlframedelimiter:
.. _imtcp.parameter.input.addtlframedelimiter:

AddtlFrameDelimiter
===================

.. index::
   single: imtcp; AddtlFrameDelimiter
   single: AddtlFrameDelimiter

.. summary-start

Specifies an additional ASCII character to use as a frame delimiter.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: AddtlFrameDelimiter
:Scope: module, input
:Type: integer
:Default: ``-1`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: 4.3.1 (module), 8.2106.0 (input)

Description
-----------
This directive permits specifying an additional frame delimiter for TCP framing. The value must be the ASCII code of the character. For example, to use the NUL character as a delimiter, set this parameter to ``0``. The standard LF delimiter remains active. To disable it, use the ``disableLFDelimiter`` parameter.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.addtlframedelimiter-usage:

.. code-block:: rsyslog

   module(load="imtcp" addtlFrameDelimiter="0")

Input usage
-----------
.. _imtcp.parameter.input.addtlframedelimiter-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" addtlFrameDelimiter="0")

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
