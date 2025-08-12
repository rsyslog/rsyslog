.. _param-omfile-filecreatemode:
.. _omfile.parameter.module.filecreatemode:

fileCreateMode
==============

.. index::
   single: omfile; fileCreateMode
   single: fileCreateMode

.. summary-start

Specifies the file system permissions for newly created log files.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: fileCreateMode
:Scope: module, action
:Type: string (octal)
:Default: module=0644; action=inherits module
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Specifies the file system permissions for log files newly created by
``omfile``. The value must be a 4-digit octal number (e.g., ``0640``).

When set at the module level, it defines the default mode for all
``omfile`` actions. When set at the action level, it overrides the module
default for that specific action.

Please note that the actual permissions depend on rsyslogd's process
umask. If in doubt, use ``$umask 0000`` at the beginning of the
configuration file to remove any restrictions.

Module usage
------------

.. _param-omfile-module-filecreatemode:
.. _omfile.parameter.module.filecreatemode-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" fileCreateMode="0644")

Action usage
------------

.. _param-omfile-action-filecreatemode:
.. _omfile.parameter.action.filecreatemode:
.. code-block:: rsyslog

   action(type="omfile" fileCreateMode="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.filecreatemode:

- $FileCreateMode — maps to fileCreateMode (status: legacy)

.. index::
   single: omfile; $FileCreateMode
   single: $FileCreateMode

See also
--------

See also :doc:`../../configuration/modules/omfile`.
