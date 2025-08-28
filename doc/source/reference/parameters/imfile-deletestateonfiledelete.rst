.. _param-imfile-deletestateonfiledelete:
.. _imfile.parameter.input.deletestateonfiledelete:
.. _imfile.parameter.deletestateonfiledelete:

deleteStateOnFileDelete
=======================

.. index::
   single: imfile; deleteStateOnFileDelete
   single: deleteStateOnFileDelete

.. summary-start

Removes the state file when the monitored file is deleted.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: deleteStateOnFileDelete
:Scope: input
:Type: boolean
:Default: on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Controls whether the state file is deleted if its associated main file is
removed. This usually prevents problems when a new file with the same name
is created. However, if a monitored file is modified with an editor that
replaces the file, deleting the state file would cause the whole file to be
reprocessed. Disable this setting only when the reason is fully understood.

Input usage
-----------
.. _param-imfile-input-deletestateonfiledelete:
.. _imfile.parameter.input.deletestateonfiledelete-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         deleteStateOnFileDelete="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.removestateondelete:

- ``removeStateOnDelete`` — maps to deleteStateOnFileDelete (status: legacy)

.. index::
   single: imfile; removeStateOnDelete
   single: removeStateOnDelete

See also
--------
See also :doc:`../../configuration/modules/imfile`.
