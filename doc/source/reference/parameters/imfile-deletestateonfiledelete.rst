.. _param-imfile-deletestateonfiledelete:
.. _imfile.parameter.module.deletestateonfiledelete:

deleteStateOnFileDelete
=======================

.. index::
   single: imfile; deleteStateOnFileDelete
   single: deleteStateOnFileDelete

.. summary-start

This parameter controls if state files are deleted if their associated main file is deleted.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: deleteStateOnFileDelete
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This parameter controls if state files are deleted if their associated
main file is deleted. Usually, this is a good idea, because otherwise
problems would occur if a new file with the same name is created. In
that case, imfile would pick up reading from the last position in
the **deleted** file, which usually is not what you want.

However, there is one situation where not deleting associated state
file makes sense: this is the case if a monitored file is modified
with an editor (like vi or gedit). Most editors write out modifications
by deleting the old file and creating a new now. If the state file
would be deleted in that case, all of the file would be reprocessed,
something that's probably not intended in most case. As a side-note,
it is strongly suggested *not* to modify monitored files with
editors. In any case, in such a situation, it makes sense to
disable state file deletion. That also applies to similar use
cases.

In general, this parameter should only by set if the users
knows exactly why this is required.

For historical reasons, this parameter has an alias called
`removeStateOnDelete`. This is to provide backward compatibility.
Newer installations should use `deleteStateOnFileDelete` only.

Input usage
-----------
.. _param-imfile-input-deletestateonfiledelete:
.. _imfile.parameter.input.deletestateonfiledelete:
.. code-block:: rsyslog

   input(type="imfile" deleteStateOnFileDelete="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.removestateondelete:
- removeStateOnDelete — maps to deleteStateOnFileDelete (status: legacy)

.. index::
   single: imfile; removeStateOnDelete
   single: removeStateOnDelete

See also
--------
See also :doc:`../../configuration/modules/imfile`.
