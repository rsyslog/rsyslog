.. _param-imjournal-usepidfromsystem:
.. _imjournal.parameter.module.usepidfromsystem:

.. meta::
   :tag: module:imjournal
   :tag: parameter:UsePidFromSystem

UsePidFromSystem
================

.. index::
   single: imjournal; UsePidFromSystem
   single: UsePidFromSystem

.. summary-start

Use ``_PID`` instead of ``SYSLOG_PID`` as the process identifier.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: UsePidFromSystem
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Retrieves the trusted ``_PID`` field from the journal instead of ``SYSLOG_PID``.
This option overrides ``UsePid`` and is deprecated; use ``UsePid="system"``
for equivalent behavior.

Module usage
------------
.. _param-imjournal-module-usepidfromsystem:
.. _imjournal.parameter.module.usepidfromsystem-usage:
.. code-block:: rsyslog

   module(load="imjournal" UsePidFromSystem="...")

Notes
-----
- Legacy documentation referred to this as a ``binary`` option.
- Deprecated; prefer ``UsePid``.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
