.. _param-imjournal-filecreatemode:
.. _imjournal.parameter.module.filecreatemode:

.. meta::
   :tag: module:imjournal
   :tag: parameter:FileCreateMode

FileCreateMode
==============

.. index::
   single: imjournal; FileCreateMode
   single: FileCreateMode

.. summary-start

Sets the octal permission mode for the state file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: FileCreateMode
:Scope: module
:Type: integer
:Default: module=0644
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Defines access permissions for the state file using a four-digit octal value.
Actual permissions also depend on the process ``umask``. If in doubt, use
``global(umask="0000")`` at the beginning of the configuration file to remove
any restrictions.

Module usage
------------
.. _param-imjournal-module-filecreatemode:
.. _imjournal.parameter.module.filecreatemode-usage:
.. code-block:: rsyslog

   module(load="imjournal" FileCreateMode="...")

Notes
-----
- Value is interpreted as an octal number.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
