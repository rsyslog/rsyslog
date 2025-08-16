.. _param-imfile-reopenontruncate:
.. _imfile.parameter.input.reopenontruncate:
.. _imfile.parameter.reopenontruncate:

reopenOnTruncate
================

.. index::
   single: imfile; reopenOnTruncate
   single: reopenOnTruncate

.. summary-start

Experimental: reopen the input file if it was truncated without changing its inode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: reopenOnTruncate
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When activated, tells rsyslog to reopen the input file if it was truncated
(the inode is unchanged but the file size on disk is smaller than the
current offset in memory). This feature is experimental.

Input usage
-----------
.. _param-imfile-input-reopenontruncate:
.. _imfile.parameter.input.reopenontruncate-usage:

.. code-block:: rsyslog

   input(type="imfile" reopenOnTruncate="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.
- This parameter is **experimental** and may change.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
