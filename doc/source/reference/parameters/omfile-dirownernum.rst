.. _param-omfile-dirownernum:
.. _omfile.parameter.module.dirownernum:

dirOwnerNum
===========

.. index::
   single: omfile; dirOwnerNum
   single: dirOwnerNum

.. summary-start

Set the file owner for directories newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dirOwnerNum
:Scope: module, action
:Type: integer
:Default: module=process user; action=system default
:Required?: no
:Introduced: 7.5.8

Description
-----------

Set the file owner for directories newly created. Please note that
this setting does not affect the owner of directories already
existing. The parameter is a numerical ID, which is used regardless
of whether the user actually exists. This can be useful if the user
mapping is not available to rsyslog during startup.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-dirownernum:
.. _omfile.parameter.module.dirownernum-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" dirOwnerNum="...")

Action usage
------------

.. _param-omfile-action-dirownernum:
.. _omfile.parameter.action.dirownernum:
.. code-block:: rsyslog

   action(type="omfile" dirOwnerNum="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.dirownernum:

- $DirOwnerNum — maps to dirOwnerNum (status: legacy)

.. index::
   single: omfile; $DirOwnerNum
   single: $DirOwnerNum

See also
--------

See also :doc:`../../configuration/modules/omfile`.
