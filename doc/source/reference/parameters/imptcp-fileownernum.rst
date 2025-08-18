.. _param-imptcp-fileownernum:
.. _imptcp.parameter.input.fileownernum:

FileOwnerNum
============

.. index::
   single: imptcp; FileOwnerNum
   single: FileOwnerNum

.. summary-start

Sets the owner of the Unix-domain socket by numeric UID.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: FileOwnerNum
:Scope: input
:Type: integer
:Default: input=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Set the file owner for the domain socket. The parameter is a numerical ID,
which is used regardless of whether the user actually exists. This can be
useful if the user mapping is not available to rsyslog during startup.

Input usage
-----------
.. _param-imptcp-input-fileownernum:
.. _imptcp.parameter.input.fileownernum-usage:

.. code-block:: rsyslog

   input(type="imptcp" fileOwnerNum="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
