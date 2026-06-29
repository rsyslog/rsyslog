Writing rsyslog Tests
=====================

The rsyslog testbench is executed via ``make check`` or ``make distcheck``. For details
on these modes, see the GNU autotools documentation. The most important thing is that
the ``make distcheck`` test execution environment is considerably different from its
``make check`` counterpart. The rsyslog testbench is crafted to handle both cases and
does so with the intensive use of environment variables.

The rsyslog testbench aims to support parallel tests. This is not yet fully implemented,
but we are working towards that goal. This has a number of implications/requirements:

* All file names, ports, etc. need to be unique.
* The ``diag.sh`` framework supports auto-generation capabilities to support this:
  use ``${RSYSLOG_DYNNAME}`` as a prefix for all files you generate. For the frequently
  used files, the framework already defines ``${RSYSLOG_OUT_LOG}`` and
  ``${RSYSLOG_OUT_LOG2}``.

When writing new tests, it is in general advisable to copy an existing test and modify
it. This also ensures you inherit the correct boilerplate and environment setup.

Test Modes
----------

The testbench supports two configuration modes for rsyslogd:

RainerScript mode (default)
  The classic mode. ``generate_conf`` writes a ``.conf`` file using
  ``module()`` / ``input()`` / ``global()`` v2 RainerScript syntax. Legacy
  ``$Directive`` syntax is still accepted but deprecated.

YAML-only mode
  Pure-YAML configuration. ``generate_conf --yaml-only`` writes a ``.yaml``
  file using the rsyslog v2 YAML loader. No RainerScript preamble is written.
  See `YAML-only mode`_ below.

Startup detection uses the imdiag port file in both modes. In RainerScript
mode, rsyslogd's own messages (startup, shutdown, errors) are also captured to
the instance's ``.started`` file via the syslogtag filter rule; tests may
use that file for content checks. In YAML-only mode no ``.started`` file is
written.

.. _yaml-only-mode:

YAML-only Mode
--------------

Use ``generate_conf --yaml-only`` when a test must validate the YAML
configuration loader or when you want to avoid any RainerScript entirely.

How It Works
~~~~~~~~~~~~

1. ``generate_conf --yaml-only [instance]`` writes
   ``${TESTCONF_NM}[instance].yaml`` containing:

   - ``version: 2``
   - ``global:`` section with ``debug.abortOnProgramError: "on"`` (and
     ``net.ipprotocol:`` if ``RSTB_FORCE_IPV4=1`` or ``RSTB_NET_IPPROTO`` is
     set)
   - ``testbench_modules:`` section that loads ``imdiag`` with all testbench
     parameters set (port file, abort timeout, queue/input timeouts)

   The ``testbench_modules:`` key is recognised by rsyslogd as an alias for
   ``modules:``. It is reserved for testbench infrastructure so it does not
   conflict with the test's own ``modules:`` section.

2. Tests add their own sections with ``add_yaml_conf``.
3. imdiag startup detection is already configured by ``generate_conf
   --yaml-only`` through module-scoped testbench parameters.

Helper Functions
~~~~~~~~~~~~~~~~

``generate_conf --yaml-only [instance]``
  Create the YAML preamble. Sets ``RSYSLOG_YAML_ONLY=1``.

``add_yaml_conf 'fragment' [instance]``
  Append a YAML fragment to ``${TESTCONF_NM}[instance].yaml``.

``add_yaml_imdiag_input [instance]``
  Historical compatibility helper. It no longer appends YAML because imdiag is
  configured by ``generate_conf --yaml-only`` as a module-scoped testbench
  listener.

Startup detection
~~~~~~~~~~~~~~~~~

Both RainerScript and yaml-only modes use the imdiag port file
(``${RSYSLOG_DYNNAME}.imdiag[instance].port``) as the sole startup signal.
``wait_startup`` polls for this file and fast-fails if rsyslogd exits before
it appears. No ``.started`` marker file is used.

Proper termination detection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Daemonized tests cannot rely on shell ``wait`` to observe the real rsyslogd
exit status because the original child exits after forking the daemon. Regular
``generate_conf`` tests configure imdiag's ``properTerminationFile`` parameter
automatically from the per-test ``RSYSLOG_DYNNAME`` basename and instance
suffix. The path is stored in rsyslog core and written as the final action
before returning from ``main``. ``wait_shutdown`` validates the marker when one
is configured and the shutdown was initiated through harness helpers that saved
the rsyslogd pid, so regular shutdown or crash-sensitive tests should use the
standard ``shutdown_when_empty`` or ``shutdown_immediate`` path instead of
adding test-local shutdown observers. Use ``set_proper_termination_file`` only
for special manual-startup or non-generated-config cases, and call
``check_proper_termination`` directly only when that special path cannot use
``wait_shutdown``.
Tests that intentionally kill rsyslogd to create dirty on-disk state must use
the testbench kill helper so the next ``wait_shutdown`` knows that no clean
main-return marker is expected for that phase.
A timeoutGuard abort writes the same file before aborting with
``status=error`` and ``reason=timeoutGuard``. The harness prints marker content
before failing so diagnostics such as ``queue.overall.size`` survive in the test
log.

The shell ``tcpflood`` helper configures a per-invocation proper termination
marker as well. ``tcpflood`` writes that marker as its final normal action, and
the wrapper validates it after a zero exit status. Direct ``./tcpflood``
invocations can opt in with ``-q <file>`` when tcpflood completion is part of
the test oracle; the option stays single-character for portability to platforms
without GNU-style long options.
Tests that intentionally provoke a tcpflood send failure should use the
``tcpflood --check-only`` wrapper form and assert the rsyslog-side oracle.
Background tcpflood clients that are expected to finish normally should use
``start_tcpflood_async`` and ``wait_tcpflood_async``. These helpers preserve the
intended concurrency while still checking the helper pid status and
proper-termination marker.

Do not add helper cleanup or observer processes to prove daemon shutdown. They
have historically caused their own races. ``timeoutGuard`` remains mandatory
hang protection and must preserve this diagnostic marker behavior when it
terminates rsyslogd.

Core dump diagnostics
~~~~~~~~~~~~~~~~~~~~~

Core files are best-effort diagnostics, not the shutdown oracle. The testbench
tries to enable core files with ``ulimit -c unlimited`` and reports the active
core policy when a test fails. On Linux this includes ``kernel.core_pattern``,
``kernel.core_uses_pid``, and ``fs.suid_dumpable`` when those files are
readable.

Generic core names such as ``core`` and ``core.*`` are not reliable ownership
evidence under parallel tests. The harness analyzes a core only when it can
attribute it to the expected process by pid/executable metadata, or when the
filename is clearly test-specific through ``RSYSLOG_DYNNAME``. If
``kernel.core_pattern`` pipes crashes to an external collector such as
``systemd-coredump`` or ``apport``, generic local core scanning is skipped
because local files may be absent, stale, or unrelated.

Absence of a local core does not weaken a missing or invalid proper termination
marker: the process still failed to reach the expected shutdown point. In that
case the harness reports that no owned local core was available. Helper
processes such as ``tcpflood`` need explicit pid or exit-status checks when
their health matters; rsyslogd core diagnostics do not prove helper health.
Use the standard ``tcpflood`` shell helper, or pass ``-q <file>`` to direct
``./tcpflood`` invocations, when tcpflood completion itself matters.
Valgrind ``vgcore.*`` files are separate from generic OS core discovery: they
belong to the supervised Valgrind run and remain a Valgrind failure signal.

Queue and Input Timeouts
~~~~~~~~~~~~~~~~~~~~~~~~

Default testbench timeout values are set via the ``RSTB_*`` environment
variables before ``generate_conf`` is called. The variables and their defaults
are:

.. list-table::
   :widths: 40 20 40
   :header-rows: 1

   * - Variable
     - Default (ms)
     - Controls
   * - ``RSTB_GLOBAL_QUEUE_SHUTDOWN_TIMEOUT``
     - 10000
     - Main message queue shutdown timeout
   * - ``RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT``
     - 60000
     - Input shutdown timeout
   * - ``RSTB_ACTION_DEFAULT_Q_TO_SHUTDOWN``
     - 20000
     - Default action queue shutdown timeout
   * - ``RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE``
     - 30000
     - Default action queue enqueue timeout
   * - ``RSTB_MAIN_Q_TO_ENQUEUE``
     - 30000
     - Main message queue enqueue timeout

These are injected as ``imdiag`` module parameters at config-load time, so
they take effect before any test-specific config is parsed. A test can
override a value by setting the variable before calling ``generate_conf``.

If a test needs a *different* value for the main queue shutdown timeout (for
example, a queue-persistence test that sets ``$MainMsgQueueTimeoutShutdown 1``
in RainerScript), it should set the value in the configuration fragment itself.
Legacy ``$MainMsgQueueTimeout*`` directives still work and override the
imdiag-supplied defaults in RainerScript mode.

Limitations in YAML-only Mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following RainerScript-specific features are not available in yaml-only
mode:

* **Legacy ``$Directive`` syntax** -- not parsed by the YAML loader.
* **``setvar_RS_HOSTNAME``** -- uses a RainerScript ``$template`` internally;
  it always runs against a RainerScript instance and its result is still
  available via ``$RS_HOSTNAME`` for yaml-only tests.

Example Test
~~~~~~~~~~~~

.. code-block:: bash

   #!/bin/bash
   . ${srcdir:=.}/diag.sh init
   require_plugin imtcp
   export NUMMESSAGES=100
   export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
   generate_conf --yaml-only

   # Test-specific modules go in a standard modules: section.
   # Testbench infrastructure (imdiag) is already in testbench_modules:.
   add_yaml_conf 'modules:'
   add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
   add_yaml_conf ''
   add_yaml_conf 'templates:'
   add_yaml_conf '  - name: outfmt'
   add_yaml_conf '    type: string'
   add_yaml_conf '    string: "%msg:F,58:2%\n"'
   add_yaml_conf ''
   add_yaml_conf 'inputs:'
   add_yaml_conf "  - type: imtcp"
   add_yaml_conf '    port: "0"'
   add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\""
   add_yaml_conf '    ruleset: main'
   add_yaml_conf ''
   add_yaml_conf 'rulesets:'
   add_yaml_conf '  - name: main'
   add_yaml_conf '    script: |'
   add_yaml_conf "      :msg, contains, \"msgnum:\" action(type=\"omfile\""
   add_yaml_conf "          template=\"outfmt\" file=\"${RSYSLOG_OUT_LOG}\")"
   startup
   tcpflood -m $NUMMESSAGES
   shutdown_when_empty
   wait_shutdown
   seq_check
   exit_test

Naming and Registration
~~~~~~~~~~~~~~~~~~~~~~~

Name yaml-only tests ``yaml-<area>-yamlonly.sh`` (or append ``-yamlonly`` to
an existing test name). Register them in ``tests/Makefile.am`` under
``TESTS_LIBYAML`` so they are skipped on systems without libyaml.
