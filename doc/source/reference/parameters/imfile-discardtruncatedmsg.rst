.. _param-imfile-discardtruncatedmsg:
.. _imfile.parameter.module.discardtruncatedmsg:

discardTruncatedMsg
===================

.. index::
   single: imfile; discardTruncatedMsg
   single: discardTruncatedMsg

.. summary-start

Controls whether the truncated part of an overly long message is discarded instead of being processed as a new message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: discardTruncatedMsg
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When messages are too long they are truncated and the following part is
processed as a new message. When this parameter is turned on the
truncated part is not processed but discarded.

Input usage
-----------
.. _param-imfile-input-discardtruncatedmsg:
.. _imfile.parameter.input.discardtruncatedmsg:
.. code-block:: rsyslog

   input(type="imfile" discardTruncatedMsg="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
