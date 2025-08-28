.. _param-imjournal-usepid:
.. _imjournal.parameter.module.usepid:

.. meta::
   :tag: module:imjournal
   :tag: parameter:UsePid

UsePid
======

.. index::
   single: imjournal; UsePid
   single: UsePid

.. summary-start

Selects which journal PID field to use: ``syslog``, ``system``, or ``both``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: UsePid
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=both
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Determines the journal field that supplies the process identifier.

``syslog``
    Use ``SYSLOG_PID``.
``system``
    Use ``_PID``.
``both``
    Try ``SYSLOG_PID`` first and fall back to ``_PID``. If neither is available,
    the message is parsed without a PID.

Module usage
------------
.. _param-imjournal-module-usepid:
.. _imjournal.parameter.module.usepid-usage:
.. code-block:: rsyslog

   module(load="imjournal" UsePid="...")

Notes
-----
- Case-insensitive values are accepted.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
