.. _param-omfile-filegroupnum:
.. _omfile.parameter.module.filegroupnum:

fileGroupNum
============

.. index::
   single: omfile; fileGroupNum
   single: fileGroupNum

.. summary-start

Set the group for files newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: fileGroupNum
:Scope: module, action
:Type: integer
:Default: module=process user's primary group; action=system default
:Required?: no
:Introduced: 7.5.8

Description
-----------

Set the group for files newly created. Please note that this setting
does not affect the group of files already existing. The parameter is
a numerical ID, which is used regardless of whether the group
actually exists. This can be useful if the group mapping is not
available to rsyslog during startup.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-filegroupnum:
.. _omfile.parameter.module.filegroupnum-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" fileGroupNum="...")

Action usage
------------

.. _param-omfile-action-filegroupnum:
.. _omfile.parameter.action.filegroupnum:
.. code-block:: rsyslog

   action(type="omfile" fileGroupNum="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.filegroupnum:

- $FileGroupNum — maps to fileGroupNum (status: legacy)

.. index::
   single: omfile; $FileGroupNum
   single: $FileGroupNum

See also
--------

See also :doc:`../../configuration/modules/omfile`.
