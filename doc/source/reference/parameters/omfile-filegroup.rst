.. _param-omfile-filegroup:
.. _omfile.parameter.module.filegroup:

fileGroup
=========

.. index::
   single: omfile; fileGroup
   single: fileGroup

.. summary-start

Set the group for files newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: fileGroup
:Scope: module, action
:Type: gid
:Default: module=process user's primary group; action=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Set the group for files newly created. Please note that this setting
does not affect the group of files already existing. The parameter is
a group name, for which the groupid is obtained by rsyslogd during
startup processing. Interim changes to the user mapping are not
detected.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-filegroup:
.. _omfile.parameter.module.filegroup-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" fileGroup="...")

Action usage
------------

.. _param-omfile-action-filegroup:
.. _omfile.parameter.action.filegroup:
.. code-block:: rsyslog

   action(type="omfile" fileGroup="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.filegroup:

- $FileGroup — maps to fileGroup (status: legacy)

.. index::
   single: omfile; $FileGroup
   single: $FileGroup

See also
--------

See also :doc:`../../configuration/modules/omfile`.
