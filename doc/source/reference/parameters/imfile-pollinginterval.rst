.. _param-imfile-pollinginterval:
.. _imfile.parameter.module.pollinginterval:

PollingInterval
===============

.. index::
   single: imfile; PollingInterval
   single: PollingInterval

.. summary-start

This setting specifies how often files are to be polled for new data.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: PollingInterval
:Scope: module
:Type: integer
:Default: module=10
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This setting specifies how often files are to be
polled for new data. For obvious reasons, it has effect only if
imfile is running in polling mode.
The time specified is in seconds. During each
polling interval, all files are processed in a round-robin fashion.

A short poll interval provides more rapid message forwarding, but
requires more system resources. While it is possible, we strongly
recommend not to set the polling interval to 0 seconds. That will
make rsyslogd become a CPU hog, taking up considerable resources. It
is supported, however, for the few very unusual situations where this
level may be needed. Even if you need quick response, 1 seconds
should be well enough. Please note that imfile keeps reading files as
long as there is any data in them. So a "polling sleep" will only
happen when nothing is left to be processed.

**We recommend to use inotify mode.**

Module usage
------------
.. _param-imfile-module-pollinginterval:
.. _imfile.parameter.module.pollinginterval-usage:
.. code-block:: rsyslog

   module(load="imfile" PollingInterval="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
