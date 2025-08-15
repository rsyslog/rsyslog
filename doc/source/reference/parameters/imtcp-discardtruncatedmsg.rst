.. _param-imtcp-discardtruncatedmsg:
.. _imtcp.parameter.module.discardtruncatedmsg:

DiscardTruncatedMsg
===================

.. index::
   single: imtcp; DiscardTruncatedMsg
   single: DiscardTruncatedMsg

.. summary-start

Discards data beyond the truncation point in octet-stuffing mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: DiscardTruncatedMsg
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Normally when a message is truncated in octet stuffing mode the part that is cut off is processed as the next message.
When this parameter is activated, the part that is cut off after a truncation is discarded and not processed.

The same-named input parameter can override this module setting.

Module usage
------------
.. _param-imtcp-module-discardtruncatedmsg:
.. _imtcp.parameter.module.discardtruncatedmsg-usage:

.. code-block:: rsyslog

   module(load="imtcp" discardTruncatedMsg="on")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
