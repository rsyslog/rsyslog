.. _param-imfile-readmode:
.. _imfile.parameter.input.readmode:
.. _imfile.parameter.readmode:

readMode
========

.. index::
   single: imfile; readMode
   single: readMode

.. summary-start

Selects a built-in method for detecting multi-line messages (0–2).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: readMode
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Provides support for processing some standard types of multi-line messages.
It is less flexible than ``startmsg.regex`` or ``endmsg.regex`` but offers
higher performance. ``readMode``, ``startmsg.regex`` and ``endmsg.regex``
cannot all be defined for the same input. The value ranges from 0 to 2:

- 0 – line based (**default**; each line is a new message)
- 1 – paragraph (blank line separates log messages)
- 2 – indented (lines starting with a space or tab belong to the previous message)

Input usage
-----------
.. _param-imfile-input-readmode:
.. _imfile.parameter.input.readmode-usage:

.. code-block:: rsyslog

   input(type="imfile" readMode="0")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilereadmode:

- ``$InputFileReadMode`` — maps to readMode (status: legacy)

.. index::
   single: imfile; $InputFileReadMode
   single: $InputFileReadMode

See also
--------
See also :doc:`../../configuration/modules/imfile`.
