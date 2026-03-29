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
3. Tests must call ``add_yaml_imdiag_input`` inside their ``inputs:`` section
   so that startup detection works.

Helper Functions
~~~~~~~~~~~~~~~~

``generate_conf --yaml-only [instance]``
  Create the YAML preamble. Sets ``RSYSLOG_YAML_ONLY=1``.

``add_yaml_conf 'fragment' [instance]``
  Append a YAML fragment to ``${TESTCONF_NM}[instance].yaml``.

``add_yaml_imdiag_input [instance]``
  Append the imdiag ``inputs:`` entry inside an already-opened ``inputs:``
  block. **Required** in every yaml-only test for startup detection.

Startup detection
~~~~~~~~~~~~~~~~~

Both RainerScript and yaml-only modes use the imdiag port file
(``${RSYSLOG_DYNNAME}.imdiag[instance].port``) as the sole startup signal.
``wait_startup`` polls for this file and fast-fails if rsyslogd exits before
it appears. No ``.started`` marker file is used.

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
   add_yaml_imdiag_input   # required -- provides startup detection
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

