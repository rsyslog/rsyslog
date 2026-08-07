.. _param-imfile-delay-message:
.. _imfile.parameter.input.delay-message:
.. _imfile.parameter.delay-message:

delay.message
=============

.. index::
   single: imfile; delay.message
   single: delay.message

.. meta::
   :description: imfile delay.message parameter reference - adds per-message delay for bandwidth rate-limiting
   :keywords: imfile, delay.message, rate-limiting, bandwidth, input plugin

.. summary-start

Adds a delay after each line read from the file, useful for rate-limiting log sources in multi-tenant environments.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: delay.message
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: 8.38.0 (via PR #2999)

Description
-----------
Instructs rsyslog to pause after reading each line from the monitored file.
The default ``0`` disables this delay.

This parameter is useful in multi-tenant environments where multiple applications
write to separate log files that are all being collected by a single rsyslog
instance. By adding a small per-message delay, no single log source can dominate
the available bandwidth for transporting logs off the host.

Unlike :doc:`imfile-maxbytesperminute` and :doc:`imfile-maxlinesperminute`, which
drop messages when rate limits are exceeded, ``delay.message`` slows down processing
without message loss. This makes it suitable for scenarios where all messages must
be preserved but bandwidth sharing is required.

The value is converted into whole seconds and remaining microseconds before
rsyslog sleeps. For example, ``delay.message="1000"`` adds a 1 millisecond
delay, while ``delay.message="1500000"`` adds a 1.5 second delay.

Input usage
-----------
.. _param-imfile-input-delay-message:
.. _imfile.parameter.input.delay-message-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/containers/app1.log"
         Tag="app1"
         delay.message="1000")

The above example configures a delay value of 1000 microseconds.

Notes
-----
- The delay is applied per-message, after each line is read from the file.
- Value is specified in microseconds (1 second = 1,000,000 microseconds).
- For bandwidth throttling without message loss, prefer this over rate-limiting
  parameters that discard messages.
- In high-throughput scenarios, even small delays can significantly reduce throughput.
  Test carefully to balance bandwidth sharing against processing lag.

See also
--------
:doc:`../../configuration/modules/imfile`
