.. _param-imfile-statefile-directory:
.. _imfile.parameter.module.statefile-directory:
.. _imfile.parameter.statefile-directory:

statefile.directory
===================

.. index::
   single: imfile; statefile.directory
   single: statefile.directory

.. summary-start

Sets a dedicated directory for imfile state files.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: statefile.directory
:Scope: module
:Type: string
:Default: module=global(workDirectory) value
:Required?: no
:Introduced: 8.1905.0

Description
-----------
Permits specification of a dedicated directory for the storage of imfile state
files. An absolute path name should be specified (e.g., ``/var/rsyslog/imfilestate``).
If not specified the global ``workDirectory`` setting is used.

**Important: The directory must exist before rsyslog is started.** rsyslog needs
write permissions to work correctly. This may also require SELinux definitions or
similar permissions in other security systems.

Module usage
------------
.. _param-imfile-module-statefile-directory:
.. _imfile.parameter.module.statefile-directory-usage:

.. code-block:: rsyslog

   module(load="imfile" statefile.directory="/var/rsyslog/imfilestate")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
