.. _param-imfile-severity:
.. _imfile.parameter.input.severity:
.. _imfile.parameter.severity:

Severity
========

.. index::
   single: imfile; Severity
   single: Severity

.. summary-start

Sets the syslog severity for lines read from the file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: Severity
:Scope: input
:Type: word
:Default: notice
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The syslog severity to be assigned to lines read. It can be specified in
textual form (for example ``info`` or ``warning``) or as a number (for
example ``6`` for ``info``). Textual form is recommended.

Input usage
-----------
.. _param-imfile-input-severity:
.. _imfile.parameter.input.severity-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         Severity="notice")

Notes
-----
- See https://en.wikipedia.org/wiki/Syslog for severity numbers.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfileseverity:

- ``$InputFileSeverity`` — maps to Severity (status: legacy)

.. index::
   single: imfile; $InputFileSeverity
   single: $InputFileSeverity

See also
--------
See also :doc:`../../configuration/modules/imfile`.
