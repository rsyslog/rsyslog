.. _param-omfile-fileowner:
.. _omfile.parameter.module.fileowner:

fileOwner
=========

.. index::
   single: omfile; fileOwner
   single: fileOwner

.. summary-start

Set the file owner for files newly created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: fileOwner
:Scope: module, action
:Type: uid
:Default: module=process user; action=system default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Set the file owner for files newly created. Please note that this
setting does not affect the owner of files already existing. The
parameter is a user name, for which the userid is obtained by
rsyslogd during startup processing. Interim changes to the user
mapping are *not* detected.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-fileowner:
.. _omfile.parameter.module.fileowner-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" fileOwner="...")

Action usage
------------

.. _param-omfile-action-fileowner:
.. _omfile.parameter.action.fileowner:
.. code-block:: rsyslog

   action(type="omfile" fileOwner="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.fileowner:

- $FileOwner — maps to fileOwner (status: legacy)

.. index::
   single: omfile; $FileOwner
   single: $FileOwner

See also
--------

See also :doc:`../../configuration/modules/omfile`.
