.. _param-imfile-maxbytesperminute:
.. _imfile.parameter.module.maxbytesperminute:

MaxBytesPerMinute
=================

.. index::
   single: imfile; MaxBytesPerMinute
   single: MaxBytesPerMinute

.. summary-start

Instructs rsyslog to enqueue a maximum number of bytes as messages per minute.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxBytesPerMinute
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Instructs rsyslog to enqueue a maximum number of bytes as messages per
minute. Once MaxBytesPerMinute is reached, subsequent messages are
discarded.

Note that messages are not truncated as a result of MaxBytesPerMinute,
rather the entire message is discarded if part of it would be above the
specified maximum bytes per minute.

The **default** value is 0, which means that no messages are discarded.

Input usage
-----------
.. _param-imfile-input-maxbytesperminute:
.. _imfile.parameter.input.maxbytesperminute:
.. code-block:: rsyslog

   input(type="imfile" MaxBytesPerMinute="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
