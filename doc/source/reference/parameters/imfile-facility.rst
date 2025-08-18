.. _param-imfile-facility:
.. _imfile.parameter.input.facility:
.. _imfile.parameter.facility:

Facility
========

.. index::
   single: imfile; Facility
   single: Facility

.. summary-start

Sets the syslog facility for messages read from this file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: Facility
:Scope: input
:Type: word
:Default: local0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The syslog facility to be assigned to messages read from this file. It can
be specified in textual form (for example ``local0``) or as a number (for
example ``16``). Textual form is recommended.

Input usage
-----------
.. _param-imfile-input-facility:
.. _imfile.parameter.input.facility-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         Facility="local0")

Notes
-----
- See https://en.wikipedia.org/wiki/Syslog for facility numbers.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilefacility:

- ``$InputFileFacility`` — maps to Facility (status: legacy)

.. index::
   single: imfile; $InputFileFacility
   single: $InputFileFacility

See also
--------
See also :doc:`../../configuration/modules/imfile`.
