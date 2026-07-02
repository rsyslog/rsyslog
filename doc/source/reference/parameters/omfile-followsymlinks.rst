.. _param-omfile-followsymlinks:
.. _omfile.parameter.module.followsymlinks:

followSymlinks
==============

.. index::
   single: omfile; followSymlinks
   single: followSymlinks

.. summary-start

Controls whether omfile follows a symbolic link in the final output path
component.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: followSymlinks
:Scope: module, action
:Type: boolean
:Default: depends on ``global(compatibility.defaults.secure=...)``
:Required?: no
:Introduced: 8.2608.0

Description
-----------

When set to ``on``, omfile keeps the historic behavior and follows a symbolic
link if the final component of the configured output path is a symlink. When
set to ``off``, omfile opens the final path component with ``O_NOFOLLOW`` where
the platform supports it, so a symlinked output file is rejected instead of
being followed.

If the parameter is not configured explicitly, the default follows
``global(compatibility.defaults.secure=...)``:

- ``strict``: do not follow the final output path component.
- ``warn``: follow the symlink and emit a warning when the final output path
  component is a symlink.
- ``backward-compatible``: follow the symlink without a warning.

This setting only applies to the final path component. Symlinks in parent
directories are not rejected by this parameter.

Action usage
------------

.. _param-omfile-action-followsymlinks:
.. _omfile.parameter.action.followsymlinks:
.. code-block:: rsyslog

   action(type="omfile" file="/var/log/app.log" followSymlinks="off")

Module usage
------------

.. code-block:: rsyslog

   module(load="builtin:omfile" followSymlinks="off")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
