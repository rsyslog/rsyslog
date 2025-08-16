.. _param-imfile-statefile:
.. _imfile.parameter.input.statefile:
.. _imfile.parameter.statefile:

stateFile
=========

.. index::
   single: imfile; stateFile
   single: stateFile

.. summary-start

Deprecated; sets a fixed state file name for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: stateFile
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the name of this file's state file. This parameter should
usually **not** be used. When wildcards are present in the monitored
file name, all matching files share the same state file, which typically
causes confusion and is unlikely to work properly. Upon startup, rsyslog
tries to detect such cases and emits warning messages, but complex
wildcard patterns may go unnoticed. For details, see
:ref:`State-Files` in the module documentation.

Input usage
-----------
.. _param-imfile-input-statefile:
.. _imfile.parameter.input.statefile-usage:

.. code-block:: rsyslog

   input(type="imfile" stateFile="/path/to/state")

Notes
-----
- Deprecated; rely on automatically generated state file names instead.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilestatefile:

- ``$InputFileStateFile`` — maps to stateFile (status: legacy)

.. index::
   single: imfile; $InputFileStateFile
   single: $InputFileStateFile

See also
--------
See also :doc:`../../configuration/modules/imfile`.
