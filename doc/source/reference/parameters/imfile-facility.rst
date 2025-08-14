.. _param-imfile-facility:
.. _imfile.parameter.module.facility:

Facility
========

.. index::
   single: imfile; Facility
   single: Facility

.. summary-start

The syslog facility to be assigned to messages read from this file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: Facility
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=local0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The syslog facility to be assigned to messages read from this file. Can be
specified in textual form (e.g. ``local0``, ``local1``, ...) or as numbers (e.g.
16 for ``local0``). Textual form is suggested. Default  is ``local0``.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog

Input usage
-----------
.. _param-imfile-input-facility:
.. _imfile.parameter.input.facility:
.. code-block:: rsyslog

   input(type="imfile" Facility="...")

Notes
-----
- Can be given as numeric code instead of name.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilefacility:
- $InputFileFacility — maps to Facility (status: legacy)

.. index::
   single: imfile; $InputFileFacility
   single: $InputFileFacility

See also
--------
See also :doc:`../../configuration/modules/imfile`.
