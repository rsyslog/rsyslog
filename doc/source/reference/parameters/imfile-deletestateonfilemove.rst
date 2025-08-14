.. _param-imfile-deletestateonfilemove:
.. _imfile.parameter.module.deletestateonfilemove:

deleteStateOnFileMove
=====================

.. index::
   single: imfile; deleteStateOnFileMove
   single: deleteStateOnFileMove

.. summary-start

If set to **on**, the state file for a monitored file is removed when that file is moved or rotated away.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: deleteStateOnFileMove
:Scope: module | input
:Type: boolean
:Default: module=off; input=inherits module
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
If set to **on**, the state file for a monitored file is removed when that file
is moved or rotated away. By default the state file is kept.

This parameter controls if state files are deleted when their associated
main file is moved or renamed. By default, this parameter inherits its
value from the module-level `deleteStateOnFileMove` parameter. When set
to "on", the state file will be automatically cleaned up when the file
is moved, preventing orphaned state files.

Module usage
------------
.. _param-imfile-module-deletestateonfilemove:
.. _imfile.parameter.module.deletestateonfilemove-usage:
.. code-block:: rsyslog

   module(load="imfile" deleteStateOnFileMove="...")

Input usage
-----------
.. _param-imfile-input-deletestateonfilemove:
.. _imfile.parameter.input.deletestateonfilemove:
.. code-block:: rsyslog

   input(type="imfile" deleteStateOnFileMove="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
