.. _param-imptcp-fileowner:
.. _imptcp.parameter.input.fileowner:

FileOwner
=========

.. index::
   single: imptcp; FileOwner
   single: FileOwner

.. summary-start

Sets the owner of the Unix-domain socket by user name.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: FileOwner
:Scope: input
:Type: UID
:Default: input=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Set the file owner for the domain socket. The parameter is a user name,
for which the userid is obtained by rsyslogd during startup processing.
Interim changes to the user mapping are *not* detected.

Input usage
-----------
.. _param-imptcp-input-fileowner:
.. _imptcp.parameter.input.fileowner-usage:

.. code-block:: rsyslog

   input(type="imptcp" fileOwner="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
