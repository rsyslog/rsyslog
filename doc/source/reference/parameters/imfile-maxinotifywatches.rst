.. _param-imfile-maxinotifywatches:
.. _imfile.parameter.module.maxinotifywatches:
.. _imfile.parameter.maxinotifywatches:

maxiNotifyWatches
=================

.. index::
   single: imfile; maxiNotifyWatches
   single: maxiNotifyWatches

.. summary-start

Specifies the maximum number of inotify watches that imfile can use.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: maxiNotifyWatches
:Scope: module
:Type: integer
:Default: module=0
:Required?: no
:Introduced: 8.2602.0

Description
-----------
This sets the maximum number of inotify watches that imfile can consume.
This is useful to limit rsyslog's consumption of inotify watches, for example, to prevent
it from using up all available watches on a system where other applications also need them.
exceeded, imfile will not be able to monitor new files. It is recommended to
set this value high enough to cover all monitored files, including those that
may be created in the future.

The system's limit can be checked via ``/proc/sys/fs/inotify/max_user_watches``.
If you do not have sufficient permissions to change the system-wide limit,
this parameter provides a way to limit rsyslog's resource consumption.

Module usage
------------
.. _imfile.parameter.module.maxinotifywatches-usage:

.. code-block:: rsyslog

   module(load="imfile" maxiNotifyWatches="256000")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
