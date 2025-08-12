.. _param-omfile-dircreatemode:
.. _omfile.parameter.module.dircreatemode:

dirCreateMode
=============

.. index::
   single: omfile; dirCreateMode
   single: dirCreateMode

.. summary-start

Sets the creation mode for directories that are automatically generated.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dirCreateMode
:Scope: module, action
:Type: string (octal)
:Default: module=0700; action=inherits module
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Sets the file system permissions for directories automatically created
by ``omfile``. The value must be a 4-digit octal number (e.g., ``0700``).

When set at the module level, it defines the default mode for all
``omfile`` actions. When set at the action level, it overrides the module
default for that specific action.

Please note that the actual permissions depend on rsyslogd's process
umask. If in doubt, use ``$umask 0000`` at the beginning of the
configuration file to remove any restrictions.

Module usage
------------

.. _param-omfile-module-dircreatemode:
.. _omfile.parameter.module.dircreatemode-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" dirCreateMode="0700")

Action usage
------------

.. _param-omfile-action-dircreatemode:
.. _omfile.parameter.action.dircreatemode:
.. code-block:: rsyslog

   action(type="omfile" dirCreateMode="0755" ...)

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.dircreatemode:

- $DirCreateMode — maps to dirCreateMode (status: legacy)

.. index::
   single: omfile; $DirCreateMode
   single: $DirCreateMode

See also
--------

See also :doc:`../../configuration/modules/omfile`.
