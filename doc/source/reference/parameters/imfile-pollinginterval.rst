.. _param-imfile-pollinginterval:
.. _imfile.parameter.module.pollinginterval:
.. _imfile.parameter.pollinginterval:

PollingInterval
===============

.. index::
   single: imfile; PollingInterval
   single: PollingInterval

.. summary-start

Seconds between file scans in polling mode; default ``10``.

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
Specifies how often files are polled for new data. It has effect only if imfile
is running in polling mode. The time specified is in seconds. During each polling
interval, all files are processed in a round-robin fashion.

A short poll interval provides more rapid message forwarding, but requires more
system resources. While it is possible, it is strongly recommended not to set the
polling interval to 0 seconds. That will make rsyslogd become a CPU hog, taking
up considerable resources. Even if quick response is needed, 1 second should be
sufficient. imfile keeps reading files as long as there is any data in them, so a
"polling sleep" will only happen when nothing is left to be processed.

**We recommend to use inotify mode.**

Module usage
------------
.. _param-imfile-module-pollinginterval:
.. _imfile.parameter.module.pollinginterval-usage:

.. code-block:: rsyslog

   module(load="imfile" PollingInterval="10")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
