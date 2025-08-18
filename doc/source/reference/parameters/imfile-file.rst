.. _param-imfile-file:
.. _imfile.parameter.input.file:
.. _imfile.parameter.file:

File
====

.. index::
   single: imfile; File
   single: File

.. summary-start

Specifies the absolute file path to monitor; wildcards are allowed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: File
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
The file being monitored. It must be an absolute path without macros or
templates. Wildcards are supported at the file name level. See
:ref:`WildCards <WildCards>` in the module documentation for details.

Input usage
-----------
.. _param-imfile-input-file:
.. _imfile.parameter.input.file-usage:

.. code-block:: rsyslog

   input(type="imfile" File="/path/to/logfile")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilename:

- ``$InputFileName`` — maps to File (status: legacy)

.. index::
   single: imfile; $InputFileName
   single: $InputFileName

See also
--------
See also :doc:`../../configuration/modules/imfile`.
