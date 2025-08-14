.. _param-imfile-statefile-directory:
.. _imfile.parameter.module.statefile-directory:
.. _imfile.parameter.module.statefile.directory:

statefile.directory
===================

.. index::
   single: imfile; statefile.directory
   single: statefile.directory

.. summary-start

Sets a dedicated directory for storing imfile state files.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: statefile.directory
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=global(WorkDirectory) value
:Required?: no
:Introduced: 8.1905.0

Description
-----------
.. versionadded:: 8.1905.0

This parameter permits to specify a dedicated directory for the storage of
imfile state files. An absolute path name should be specified (e.g.
`/var/rsyslog/imfilestate`). This permits to keep imfile state files separate
from other rsyslog work items.

If not specified the global `workDirectory` setting is used.

**Important: The directory must exist before rsyslog is started.** Also,
rsyslog needs write permissions to work correctly. Keep in mind that this
also might require SELinux definitions (or similar for other enhanced security
systems).

Module usage
------------
.. _param-imfile-module-statefile-directory:
.. _imfile.parameter.module.statefile-directory-usage:
.. code-block:: rsyslog

   module(load="imfile" statefile.directory="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
