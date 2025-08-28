.. _param-imptcp-filecreatemode:
.. _imptcp.parameter.input.filecreatemode:

FileCreateMode
==============

.. index::
   single: imptcp; FileCreateMode
   single: FileCreateMode

.. summary-start

Sets access permissions for the Unix-domain socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: FileCreateMode
:Scope: input
:Type: octalNumber
:Default: input=0644
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Set the access permissions for the domain socket. The value given must
always be a 4-digit octal number, with the initial digit being zero.
Please note that the actual permission depend on rsyslogd's process
umask. If in doubt, use "``$umask 0000``" right at the beginning of the
configuration file to remove any restrictions.

Input usage
-----------
.. _param-imptcp-input-filecreatemode:
.. _imptcp.parameter.input.filecreatemode-usage:

.. code-block:: rsyslog

   input(type="imptcp" fileCreateMode="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
