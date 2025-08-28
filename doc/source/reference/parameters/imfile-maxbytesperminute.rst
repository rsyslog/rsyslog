.. _param-imfile-maxbytesperminute:
.. _imfile.parameter.input.maxbytesperminute:
.. _imfile.parameter.maxbytesperminute:

MaxBytesPerMinute
=================

.. index::
   single: imfile; MaxBytesPerMinute
   single: MaxBytesPerMinute

.. summary-start

Drops messages when the total bytes per minute exceed the specified limit.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxBytesPerMinute
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Instructs rsyslog to enqueue only up to the specified number of bytes as
messages per minute. Once the limit is reached, subsequent messages are
discarded. Messages are not truncated; an entire message is dropped if it
would exceed the limit. The default ``0`` disables this check.

Input usage
-----------
.. _param-imfile-input-maxbytesperminute:
.. _imfile.parameter.input.maxbytesperminute-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         MaxBytesPerMinute="0")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
