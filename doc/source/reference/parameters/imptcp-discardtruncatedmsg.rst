.. _param-imptcp-discardtruncatedmsg:
.. _imptcp.parameter.input.discardtruncatedmsg:

DiscardTruncatedMsg
===================

.. index::
   single: imptcp; DiscardTruncatedMsg
   single: DiscardTruncatedMsg

.. summary-start

Discards the remaining part of a message after truncation.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: DiscardTruncatedMsg
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
When a message is split because it is too long the second part is normally
processed as the next message. This can cause problems. When this parameter
is turned on the part of the message after the truncation will be discarded.

Input usage
-----------
.. _param-imptcp-input-discardtruncatedmsg:
.. _imptcp.parameter.input.discardtruncatedmsg-usage:

.. code-block:: rsyslog

   input(type="imptcp" discardTruncatedMsg="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
