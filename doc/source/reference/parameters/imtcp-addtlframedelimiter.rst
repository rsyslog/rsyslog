.. _param-imtcp-addtlframedelimiter:
.. _imtcp.parameter.module.addtlframedelimiter:

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
:Scope: module
:Type: integer
:Default: module=-1
:Required?: no
:Introduced: 4.3.1

Description
-----------
This directive permits to specify an additional frame delimiter for
Multiple receivers may be configured by specifying $InputTCPServerRun
multiple times.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-addtlframedelimiter:
.. _imtcp.parameter.module.addtlframedelimiter-usage:

.. code-block:: rsyslog

   module(load="imtcp" addtlFrameDelimiter="...")

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

