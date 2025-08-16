.. _param-imfile-maxlinesperminute:
.. _imfile.parameter.input.maxlinesperminute:
.. _imfile.parameter.maxlinesperminute:

MaxLinesPerMinute
=================

.. index::
   single: imfile; MaxLinesPerMinute
   single: MaxLinesPerMinute

.. summary-start

Limits how many lines per minute are enqueued as messages; excess lines are discarded.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxLinesPerMinute
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Instructs rsyslog to enqueue up to the specified maximum number of lines as
messages per minute. Lines above this value are discarded. The default ``0``
means no lines are discarded.

Input usage
-----------
.. _param-imfile-input-maxlinesperminute:
.. _imfile.parameter.input.maxlinesperminute-usage:

.. code-block:: rsyslog

   input(type="imfile" MaxLinesPerMinute="0")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
