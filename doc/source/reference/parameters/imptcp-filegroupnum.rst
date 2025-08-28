.. _param-imptcp-filegroupnum:
.. _imptcp.parameter.input.filegroupnum:

FileGroupNum
============

.. index::
   single: imptcp; FileGroupNum
   single: FileGroupNum

.. summary-start

Sets the group of the Unix-domain socket by numeric GID.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: FileGroupNum
:Scope: input
:Type: integer
:Default: input=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Set the group for the domain socket. The parameter is a numerical ID,
which is used regardless of whether the group actually exists. This can
be useful if the group mapping is not available to rsyslog during startup.

Input usage
-----------
.. _param-imptcp-input-filegroupnum:
.. _imptcp.parameter.input.filegroupnum-usage:

.. code-block:: rsyslog

   input(type="imptcp" fileGroupNum="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
