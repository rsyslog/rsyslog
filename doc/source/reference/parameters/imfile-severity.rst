.. _param-imfile-severity:
.. _imfile.parameter.module.severity:

Severity
========

.. index::
   single: imfile; Severity
   single: Severity

.. summary-start

The syslog severity to be assigned to lines read.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: Severity
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=notice
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The syslog severity to be assigned to lines read. Can be specified
in textual   form (e.g. ``info``, ``warning``, ...) or as numbers (e.g. 6
for ``info``). Textual form is suggested. Default is ``notice``.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog

Input usage
-----------
.. _param-imfile-input-severity:
.. _imfile.parameter.input.severity:
.. code-block:: rsyslog

   input(type="imfile" Severity="...")

Notes
-----
- Can be given as numeric code instead of name.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfileseverity:
- $InputFileSeverity — maps to Severity (status: legacy)

.. index::
   single: imfile; $InputFileSeverity
   single: $InputFileSeverity

See also
--------
See also :doc:`../../configuration/modules/imfile`.
