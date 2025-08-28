.. _param-imptcp-filegroup:
.. _imptcp.parameter.input.filegroup:

FileGroup
=========

.. index::
   single: imptcp; FileGroup
   single: FileGroup

.. summary-start

Sets the group of the Unix-domain socket by group name.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: FileGroup
:Scope: input
:Type: GID
:Default: input=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Set the group for the domain socket. The parameter is a group name, for
which the groupid is obtained by rsyslogd during startup processing.
Interim changes to the user mapping are not detected.

Input usage
-----------
.. _param-imptcp-input-filegroup:
.. _imptcp.parameter.input.filegroup-usage:

.. code-block:: rsyslog

   input(type="imptcp" fileGroup="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
