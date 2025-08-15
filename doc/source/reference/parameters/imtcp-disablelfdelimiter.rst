.. _param-imtcp-disablelfdelimiter:
.. _imtcp.parameter.module.disablelfdelimiter:

DisableLFDelimiter
==================

.. index::
   single: imtcp; DisableLFDelimiter
   single: DisableLFDelimiter

.. summary-start

Disables LF as a frame delimiter to allow a custom delimiter.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: DisableLFDelimiter
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Industry-standard plain text tcp syslog uses the LF to delimit
syslog frames. However, some users brought up the case that it may be
useful to define a different delimiter and totally disable LF as a
delimiter (the use case named were multi-line messages). This mode is
non-standard and will probably come with a lot of problems. However,
as there is need for it and it is relatively easy to support, we do
so. Be sure to turn this setting to "on" only if you exactly know
what you are doing. You may run into all sorts of troubles, so be
prepared to wrangle with that!

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-disablelfdelimiter:
.. _imtcp.parameter.module.disablelfdelimiter-usage:

.. code-block:: rsyslog

   module(load="imtcp" disableLFDelimiter="on")

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

