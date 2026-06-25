.. _param-imdiag-properterminationfile:
.. _imdiag.parameter.module.properterminationfile:

ProperTerminationFile
=====================

.. index::
   single: imdiag; ProperTerminationFile

.. summary-start

Configures the testbench-only file that rsyslogd writes as its final normal
process-exit action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ProperTerminationFile
:Scope: module
:Type: string
:Default: unset
:Required?: no

Description
-----------

``ProperTerminationFile`` is intended for daemonized rsyslog testbench
scenarios where the shell cannot use ``wait`` to observe the real daemon
process exit status. When set, imdiag passes the path to rsyslog core-owned
storage. If rsyslog reaches the final normal ``main`` return path, the core
writes the file as the last action before returning to the operating system.

The marker file is opened with ``O_NOFOLLOW`` and mode ``0600``. If the file
cannot be opened, written, or closed, rsyslog writes a concise diagnostic to
stderr and exits with a non-zero status. No rsyslog internal warning or error
message is emitted for marker write failures.

A valid clean-shutdown file contains line-oriented fields such as
``status=ok`` and ``phase=main-return``. TimeoutGuard writes the same file
before aborting with ``status=error``, ``reason=timeoutGuard``, and
``phase=timeoutguard-abort``. Marker content also includes
``queue.overall.size`` so failing tests retain basic queue backlog context.
Absence of the file means the daemon did not prove a proper final shutdown and
testbench code should treat that as crash or abort evidence.

Module usage
------------

.. code-block:: rsyslog

   module(load="imdiag" properTerminationFile="./rstb.example.proper-termination")

YAML usage
----------

.. code-block:: yaml

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       properterminationfile: "./rstb.example.proper-termination"

See also :doc:`../../configuration/modules/imdiag`.
