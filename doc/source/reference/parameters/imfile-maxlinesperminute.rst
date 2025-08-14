.. _param-imfile-maxlinesperminute:
.. _imfile.parameter.module.maxlinesperminute:

MaxLinesPerMinute
=================

.. index::
   single: imfile; MaxLinesPerMinute
   single: MaxLinesPerMinute

.. summary-start

Instructs rsyslog to enqueue up to the specified maximum number of lines as messages per minute.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxLinesPerMinute
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Instructs rsyslog to enqueue up to the specified maximum number of lines
as messages per minute. Lines above this value are discarded.

The **default** value is 0, which means that no lines are discarded.

Input usage
-----------
.. _param-imfile-input-maxlinesperminute:
.. _imfile.parameter.input.maxlinesperminute:
.. code-block:: rsyslog

   input(type="imfile" MaxLinesPerMinute="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
