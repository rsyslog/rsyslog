.. _param-omfile-dirgroupnum:
.. _omfile.parameter.module.dirgroupnum:

dirGroupNum
===========

.. index::
   single: omfile; dirGroupNum
   single: dirGroupNum

.. summary-start

Set the group for directories newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dirGroupNum
:Scope: module, action
:Type: integer
:Default: module=process user's primary group; action=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Set the group for directories newly created. Please note that this
setting does not affect the group of directories already existing.
The parameter is a numerical ID, which is used regardless of whether
the group actually exists. This can be useful if the group mapping is
not available to rsyslog during startup.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-dirgroupnum:
.. _omfile.parameter.module.dirgroupnum-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" dirGroupNum="...")

Action usage
------------

.. _param-omfile-action-dirgroupnum:
.. _omfile.parameter.action.dirgroupnum:
.. code-block:: rsyslog

   action(type="omfile" dirGroupNum="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.dirgroupnum:

- $DirGroupNum — maps to dirGroupNum (status: legacy)

.. index::
   single: omfile; $DirGroupNum
   single: $DirGroupNum

See also
--------

See also :doc:`../../configuration/modules/omfile`.
