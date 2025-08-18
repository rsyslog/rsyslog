.. _param-imfile-discardtruncatedmsg:
.. _imfile.parameter.input.discardtruncatedmsg:
.. _imfile.parameter.discardtruncatedmsg:

discardTruncatedMsg
===================

.. index::
   single: imfile; discardTruncatedMsg
   single: discardTruncatedMsg

.. summary-start

Discards the truncated part of an overlong message instead of processing it.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: discardTruncatedMsg
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When messages are too long they are truncated and the remaining part is
processed as a new message. If ``discardTruncatedMsg`` is ``on``, the
truncated part is discarded instead.

Input usage
-----------
.. _param-imfile-input-discardtruncatedmsg:
.. _imfile.parameter.input.discardtruncatedmsg-usage:

.. code-block:: rsyslog

   input(type="imfile" discardTruncatedMsg="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
