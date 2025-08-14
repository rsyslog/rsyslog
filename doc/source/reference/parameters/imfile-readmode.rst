.. _param-imfile-readmode:
.. _imfile.parameter.module.readmode:

readMode
========

.. index::
   single: imfile; readMode
   single: readMode

.. summary-start

This provides support for processing some standard types of multiline messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: readMode
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This provides support for processing some standard types of multiline
messages. It is less flexible than ``startmsg.regex`` or ``endmsg.regex`` but
offers higher performance than regex processing. Note that ``readMode`` and
``startmsg.regex`` and ``endmsg.regex`` cannot all be defined for the same
input.

The value can range from 0-2 and determines the multiline
detection method.

0 - (**default**) line based (each line is a new message)

1 - paragraph (There is a blank line between log messages)

2 - indented (new log messages start at the beginning of a line. If a
line starts with a space or tab "\t" it is part of the log message before it)

Input usage
-----------
.. _param-imfile-input-readmode:
.. _imfile.parameter.input.readmode:
.. code-block:: rsyslog

   input(type="imfile" readMode="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilereadmode:
- $InputFileReadMode — maps to readMode (status: legacy)

.. index::
   single: imfile; $InputFileReadMode
   single: $InputFileReadMode

See also
--------
See also :doc:`../../configuration/modules/imfile`.
