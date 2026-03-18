.. _param-omazuredce-flush_timeout_ms:
.. _omazuredce.parameter.action.flush_timeout_ms:

.. meta::
   :description: Reference for the omazuredce flush_timeout_ms parameter.
   :keywords: rsyslog, omazuredce, flush_timeout_ms, azure, batching

flush_timeout_ms
================

.. index::
   single: omazuredce; flush_timeout_ms
   single: flush_timeout_ms

.. summary-start

Controls how long a partially filled batch may stay idle before it is flushed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: flush_timeout_ms
:Scope: action
:Type: non-negative integer
:Default: 1000
:Required?: no
:Introduced: Not specified

Description
-----------
When ``flush_timeout_ms`` is greater than ``0``, a worker thread checks for
idle batches and flushes them after the configured number of milliseconds.

When it is set to ``0``, the timer-based flush is disabled and batches are sent
only when they fill up or when the current action queue transaction ends.

Action usage
------------
.. _omazuredce.parameter.action.flush_timeout_ms-usage:

.. code-block:: rsyslog

   action(type="omazuredce" flush_timeout_ms="2000" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
