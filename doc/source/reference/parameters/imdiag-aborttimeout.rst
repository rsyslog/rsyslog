.. _param-imdiag-aborttimeout:
.. _imdiag.parameter.module.aborttimeout:

AbortTimeout
============

.. index::
   single: imdiag; AbortTimeout
   single: AbortTimeout

.. summary-start

Starts a watchdog thread that aborts rsyslog if it runs longer than the configured time limit.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: AbortTimeout
:Scope: module
:Type: integer (seconds)
:Default: module=none (disabled)
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
When set, :samp:`AbortTimeout` installs a guard thread that tracks the runtime
of the rsyslog instance. The guard begins timing as soon as the configuration
that contains the parameter is loaded, not just during shutdown. If rsyslog
remains active for longer than the configured number of seconds after the guard
starts, the thread writes a status message to ``stderr`` and terminates the
daemon with ``abort()``. The guard is intended for automated test environments
to detect deadlocks or hangs.

The guard can only be configured once during the lifetime of the process. A
second attempt to configure the watchdog is ignored and logs an error. Values
less than or equal to zero are rejected.

Module usage
------------
.. _param-imdiag-module-aborttimeout:
.. _imdiag.parameter.module.aborttimeout-usage:

.. code-block:: rsyslog

   module(load="imdiag" abortTimeout="600")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiagaborttimeout:

- $IMDiagAbortTimeout — maps to AbortTimeout (status: legacy)

.. index::
   single: imdiag; $IMDiagAbortTimeout
   single: $IMDiagAbortTimeout

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
