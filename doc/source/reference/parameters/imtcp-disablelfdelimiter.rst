.. _param-imtcp-disablelfdelimiter:
.. _imtcp.parameter.module.disablelfdelimiter:
.. _imtcp.parameter.input.disablelfdelimiter:

DisableLFDelimiter
==================

.. index::
   single: imtcp; DisableLFDelimiter
   single: DisableLFDelimiter

.. summary-start

Disables the industry-standard LF character as a syslog frame delimiter.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: DisableLFDelimiter
:Scope: module, input
:Type: boolean
:Default: ``off`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
Industry-standard plain text TCP syslog uses the LF character to delimit syslog frames. However, in some cases it may be useful to define a different delimiter and totally disable LF as a delimiter (e.g., for multi-line messages). This mode is non-standard and can cause interoperability issues. Be sure to turn this setting to "on" only if you know exactly what you are doing.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.disablelfdelimiter-usage:

.. code-block:: rsyslog

   module(load="imtcp" disableLFDelimiter="on")

Input usage
-----------
.. _imtcp.parameter.input.disablelfdelimiter-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" disableLFDelimiter="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverdisablelfdelimiter:
- $InputTCPServerDisableLFDelimiter — maps to DisableLFDelimiter (status: legacy)

.. index::
   single: imtcp; $InputTCPServerDisableLFDelimiter
   single: $InputTCPServerDisableLFDelimiter

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
