.. _param-omfile-dirowner:
.. _omfile.parameter.module.dirowner:

dirOwner
========

.. index::
   single: omfile; dirOwner
   single: dirOwner

.. summary-start

Set the file owner for directories newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dirOwner
:Scope: module, action
:Type: uid
:Default: module=process user; action=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Set the file owner for directories newly created. Please note that
this setting does not affect the owner of directories already
existing. The parameter is a user name, for which the userid is
obtained by rsyslogd during startup processing. Interim changes to
the user mapping are not detected.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-dirowner:
.. _omfile.parameter.module.dirowner-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" dirOwner="...")

Action usage
------------

.. _param-omfile-action-dirowner:
.. _omfile.parameter.action.dirowner:
.. code-block:: rsyslog

   action(type="omfile" dirOwner="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.dirowner:

- $DirOwner — maps to dirOwner (status: legacy)

.. index::
   single: omfile; $DirOwner
   single: $DirOwner

See also
--------

See also :doc:`../../configuration/modules/omfile`.
