.. _param-imfile-reopenontruncate:
.. _imfile.parameter.module.reopenontruncate:

reopenOnTruncate
================

.. index::
   single: imfile; reopenOnTruncate
   single: reopenOnTruncate

.. summary-start

This is an **experimental** feature that tells rsyslog to reopen the input file when it was truncated (inode unchanged but file size on disk is less than current offset in memory).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: reopenOnTruncate
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is an **experimental** feature that tells rsyslog to reopen input file
when it was truncated (inode unchanged but file size on disk is less than
current offset in memory).

Input usage
-----------
.. _param-imfile-input-reopenontruncate:
.. _imfile.parameter.input.reopenontruncate:
.. code-block:: rsyslog

   input(type="imfile" reopenOnTruncate="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
