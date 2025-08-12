.. _param-omfile-dirgroup:
.. _omfile.parameter.module.dirgroup:

dirGroup
========

.. index::
   single: omfile; dirGroup
   single: dirGroup

.. summary-start

Set the group for directories newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dirGroup
:Scope: module, action
:Type: gid
:Default: module=process user's primary group; action=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Set the group for directories newly created. Please note that this
setting does not affect the group of directories already existing.
The parameter is a group name, for which the groupid is obtained by
rsyslogd on during startup processing. Interim changes to the user
mapping are not detected.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-dirgroup:
.. _omfile.parameter.module.dirgroup-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" dirGroup="...")

Action usage
------------

.. _param-omfile-action-dirgroup:
.. _omfile.parameter.action.dirgroup:
.. code-block:: rsyslog

   action(type="omfile" dirGroup="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.dirgroup:

- $DirGroup — maps to dirGroup (status: legacy)

.. index::
   single: omfile; $DirGroup
   single: $DirGroup

See also
--------

See also :doc:`../../configuration/modules/omfile`.
