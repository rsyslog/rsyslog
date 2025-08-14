.. _param-imfile-file:
.. _imfile.parameter.module.file:

File
====

.. index::
   single: imfile; File
   single: File

.. summary-start

The file being monitored.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: File
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
The file being monitored. So far, this must be an absolute name (no
macros or templates). Note that wildcards are supported at the file
name level (see **WildCards** below for more details).

Input usage
-----------
.. _param-imfile-input-file:
.. _imfile.parameter.input.file:
.. code-block:: rsyslog

   input(type="imfile" File="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilename:
- $InputFileName — maps to File (status: legacy)

.. index::
   single: imfile; $InputFileName
   single: $InputFileName

See also
--------
See also :doc:`../../configuration/modules/imfile`.
