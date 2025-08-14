.. _param-imfile-maxlinesatonce:
.. _imfile.parameter.module.maxlinesatonce:

MaxLinesAtOnce
==============

.. index::
   single: imfile; MaxLinesAtOnce
   single: MaxLinesAtOnce

.. summary-start

This is a legacy setting that only is supported in *polling* mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxLinesAtOnce
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This is a legacy setting that only is supported in *polling* mode.
In *inotify* mode, it is fixed at 0 and all attempts to configure
a different value will be ignored, but will generate an error
message.

Please note that future versions of imfile may not support this
parameter at all. So it is suggested to not use it.

In *polling* mode, if set to 0, each file will be fully processed and
then processing switches to the next file. If it is set to any other
value, a maximum of [number] lines is processed in sequence for each file,
and then the file is switched. This provides a kind of multiplexing
the load of multiple files and probably leads to a more natural
distribution of events when multiple busy files are monitored. For
*polling* mode, the **default** is 10240.

Input usage
-----------
.. _param-imfile-input-maxlinesatonce:
.. _imfile.parameter.input.maxlinesatonce:
.. code-block:: rsyslog

   input(type="imfile" MaxLinesAtOnce="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilemaxlinesatonce:
- $InputFileMaxLinesAtOnce — maps to MaxLinesAtOnce (status: legacy)

.. index::
   single: imfile; $InputFileMaxLinesAtOnce
   single: $InputFileMaxLinesAtOnce

See also
--------
See also :doc:`../../configuration/modules/imfile`.
