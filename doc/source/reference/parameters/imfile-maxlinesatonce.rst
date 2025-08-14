.. _param-imfile-maxlinesatonce:
.. _imfile.parameter.input.maxlinesatonce:
.. _imfile.parameter.maxlinesatonce:

MaxLinesAtOnce
==============

.. index::
   single: imfile; MaxLinesAtOnce
   single: MaxLinesAtOnce

.. summary-start

Legacy polling-mode limit on how many lines are read before switching files.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxLinesAtOnce
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This legacy setting is only supported in polling mode. In inotify mode it is
fixed at ``0`` and attempts to set a different value are ignored but generate
an error message. Future versions may remove this parameter.

In polling mode, a value of ``0`` processes each file completely before
switching to the next. Any other value limits processing to the specified
number of lines before moving to the next file, providing a form of load
multiplexing. For polling mode, the default is ``10240``.

Input usage
-----------
.. _param-imfile-input-maxlinesatonce:
.. _imfile.parameter.input.maxlinesatonce-usage:

.. code-block:: rsyslog

   input(type="imfile" MaxLinesAtOnce="0")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilemaxlinesatonce:
- ``$InputFileMaxLinesAtOnce`` — maps to MaxLinesAtOnce (status: legacy)

.. index::
   single: imfile; $InputFileMaxLinesAtOnce
   single: $InputFileMaxLinesAtOnce

See also
--------
See also :doc:`../../configuration/modules/imfile`.
