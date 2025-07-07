writing rsyslog tests
=====================

The rsyslog testbench is executed via `make check` or `make distcheck`. For details, on
these modes, see the GNU autotools documentation. The most important thing is that
the `make distcheck` test execution environment is considerably different from its
`make check` counterpart. The rsyslog testbench is crafted to handle both cases and
does so with the (intensive) use of environment variables.

The rsyslog testbench aims to support parallel tests. This is not yet fully implemented,
but we are working towards that goal. This has a number of implications/requirements:

* all file names, ports, etc need to be unique
* the diag.sh framework supports auto-generation capabilities to support this:
  use `${RSYSLOG_DYNNAME}` a prefix for all files you generate. For the frequently
  used files, the framework already defines  `${RSYSLOG_OUT_LOG}` and `${RSYSLOG_OUT_LOG2}` 


When writing new tests, it is in general advisable to copy an existing test and change
it. This also helps you get requirements files.
