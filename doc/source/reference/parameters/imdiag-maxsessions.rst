.. _param-imdiag-maxsessions:
.. _imdiag.parameter.module.maxsessions:

MaxSessions
===========

.. index::
   single: imdiag; MaxSessions
   single: MaxSessions

.. summary-start

Limits the number of concurrent diagnostic control connections accepted.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: MaxSessions
:Scope: module
:Type: integer
:Default: module=20
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Defines how many client sessions the diagnostic listener accepts at once. By
default the listener allows 20 concurrent sessions. The limit must be set
before :ref:`ServerRun <param-imdiag-serverrun>` creates the TCP listener.
Additional connection attempts beyond the configured number are rejected.

Module usage
------------
.. _param-imdiag-module-maxsessions:
.. _imdiag.parameter.module.maxsessions-usage:

.. code-block:: rsyslog

   module(load="imdiag" maxSessions="10")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiagmaxsessions:

- $IMDiagMaxSessions — maps to MaxSessions (status: legacy)

.. index::
   single: imdiag; $IMDiagMaxSessions
   single: $IMDiagMaxSessions

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
