.. _param-imuxsock-createpath:
.. _imuxsock.parameter.input.createpath:

CreatePath
==========

.. index::
   single: imuxsock; CreatePath
   single: CreatePath

.. summary-start

Creates missing directories for the specified socket path.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: CreatePath
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 4.7.0

Description
-----------
Create directories in the socket path if they do not already exist.
They are created with 0755 permissions with the owner being the
process under which rsyslogd runs. The default is not to create
directories. Keep in mind, though, that rsyslogd always creates
the socket itself if it does not exist (just not the directories
by default).

This option is primarily considered useful for defining additional
sockets that reside on non-permanent file systems. As rsyslogd probably
starts up before the daemons that create these sockets, it is a vehicle
to enable rsyslogd to listen to those sockets even though their directories
do not yet exist.

.. versionadded:: 4.7.0

Input usage
-----------
.. _param-imuxsock-input-createpath:
.. _imuxsock.parameter.input.createpath-usage:

.. code-block:: rsyslog

   input(type="imuxsock" createPath="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.inputunixlistensocketcreatepath:

- $InputUnixListenSocketCreatePath — maps to CreatePath (status: legacy)

.. index::
   single: imuxsock; $InputUnixListenSocketCreatePath
   single: $InputUnixListenSocketCreatePath

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
