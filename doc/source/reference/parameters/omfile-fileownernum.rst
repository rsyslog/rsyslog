.. _param-omfile-fileownernum:
.. _omfile.parameter.module.fileownernum:

fileOwnerNum
============

.. index::
   single: omfile; fileOwnerNum
   single: fileOwnerNum

.. summary-start

Set the file owner for files newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: fileOwnerNum
:Scope: module, action
:Type: integer
:Default: module=process user; action=system default
:Required?: no
:Introduced: 7.5.8

Description
-----------

Set the file owner for files newly created. Please note that this
setting does not affect the owner of files already existing. The
parameter is a numerical ID, which is used regardless of
whether the user actually exists. This can be useful if the user
mapping is not available to rsyslog during startup.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-fileownernum:
.. _omfile.parameter.module.fileownernum-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" fileOwnerNum="...")

Action usage
------------

.. _param-omfile-action-fileownernum:
.. _omfile.parameter.action.fileownernum:
.. code-block:: rsyslog

   action(type="omfile" fileOwnerNum="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.fileownernum:

- $FileOwnerNum — maps to fileOwnerNum (status: legacy)

.. index::
   single: omfile; $FileOwnerNum
   single: $FileOwnerNum

See also
--------

See also :doc:`../../configuration/modules/omfile`.
