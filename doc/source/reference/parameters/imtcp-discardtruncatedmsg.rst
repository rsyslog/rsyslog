.. _param-imtcp-discardtruncatedmsg:
.. _imtcp.parameter.module.discardtruncatedmsg:
.. _imtcp.parameter.input.discardtruncatedmsg:

DiscardTruncatedMsg
===================

.. index::
   single: imtcp; DiscardTruncatedMsg
   single: DiscardTruncatedMsg

.. summary-start

Discards the remaining part of a message truncated in octet-stuffing mode.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: DiscardTruncatedMsg
:Scope: module, input
:Type: boolean
:Default: ``off`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
Normally, when a message is truncated in octet-stuffing mode, the part that is cut off is processed as the next message. When this parameter is activated, the truncated part is discarded and not processed.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.discardtruncatedmsg-usage:

.. code-block:: rsyslog

   module(load="imtcp" discardTruncatedMsg="on")

Input usage
-----------
.. _imtcp.parameter.input.discardtruncatedmsg-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" discardTruncatedMsg="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
