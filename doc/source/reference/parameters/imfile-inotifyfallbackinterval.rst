.. _param-imfile-inotifyfallbackinterval:
.. _imfile.parameter.module.inotifyfallbackinterval:
.. _imfile.parameter.inotifyfallbackinterval:

inotifyFallbackInterval
=======================

.. index::
   single: imfile; inotifyFallbackInterval
   single: inotifyFallbackInterval

.. summary-start

Sets the interval in seconds for fallback rescans when imfile cannot add inotify watches.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: inotifyFallbackInterval
:Scope: module
:Type: integer
:Default: module=60
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When imfile runs in inotify mode and cannot allocate new watches (for example,
because the module limit or the system limit was reached), it switches to
periodic rescans to discover changes and retry arming watches. This parameter
controls the rescan interval in seconds. Set it to ``0`` to disable fallback
rescans.

Module usage
------------
.. _imfile.parameter.module.inotifyfallbackinterval-usage:

.. code-block:: rsyslog

   module(load="imfile" inotifyFallbackInterval="60")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
