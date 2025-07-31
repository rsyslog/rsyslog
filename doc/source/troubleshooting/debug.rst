Rsyslog Debug Support
=====================

For harder to find issues, rsyslog has integrated debug support. Usually,
this is not required for finding configuration issues but rather
to hunt for program or plugin bugs. However, there are several
occasions where debug log has proven to be quite helpful in finding
out configuration issues.

A :doc:`quick guide can be found here<../troubleshooting/howtodebug>`.

Signals supported
-----------------

**SIGUSR1** - turns debug messages on and off. Note that for this signal
to work, rsyslogd must be running with debugging enabled, either via the
-d command line switch or the environment options specified below. It is
**not** required that rsyslog was compiled with debugging enabled (but
depending on the settings this may lead to better debug info).

**Note:** this signal **may go away** in later releases and may be
replaced by something else.

Environment Variables
---------------------

There are two environment variables that set several debug settings:

-  The "RSYSLOG\_DEBUGLOG" (sample:
    RSYSLOG\_DEBUGLOG="/path/to/debuglog/debug.log") writes (almost) all debug
   message to the specified log file in addition to stdout. Some system
   messages (e.g. segfault or abort message) are not written to the file
   as we can not capture them.
-  Runtime debug support is controlled by "RSYSLOG\_DEBUG".

   The "RSYSLOG\_DEBUG" environment variable contains an option string
   with the following options possible (all are case insensitive):

   -  **NoLogTimeStamp** - do not prefix log lines with a timestamp
      (default is to do that).
   -  **NoStdOut** - do not emit debug messages to stdout. If
      RSYSLOG\_DEBUGLOG is not set, this means no messages will be
      displayed at all.
   -  **Debug** - if present, turns on the debug system and enables
      debug output
   -  **DebugOnDemand** - if present, turns on the debug system but does
      not enable debug output itself. You need to send SIGUSR1 to turn
      it on when desired.
   -  **OutputTidToStderr** - if present, makes rsyslog output
      information about the thread id (tid) of newly created processes to
      stderr. Note that not necessarily all new threads are reported
      (depends on the code, e.g. of plugins). This is only available
      under Linux. This usually does NOT work when privileges have been
      dropped (that's not a bug, but the way it is).
   -  **help** - display a very short list of commands - hopefully a
      life saver if you can't access the documentation...

   Individual options are separated by spaces.

Why Environment Variables?
~~~~~~~~~~~~~~~~~~~~~~~~~~

You may ask why we use environment variables for debug-system parameters
and not the usual rsyslog.conf configuration commands. After all,
environment variables force one to change distro-specific configuration
files, whereas regular configuration directives would fit nicely into
the one central rsyslog.conf.

Historically environment variables were necessary to initialize so-called
"rtinst" mode. This mode no longer exists, as the OS tools have improved.
Using environment variables still has the benefit that the work right from
initialization of rsyslogd. Most importantly, this is before the rsyslog.conf
is read.

If that is no issue, rsyslog.conf global statements can be used to enable
debug mode and provide some settings.

HOWEVER, if you have a too hard time to set debug instructions using the
environment variables, there is a cure, described in the next paragraph.

Enabling Debug via rsyslog.conf
-------------------------------

As described in the previous paragraph, enabling debug via rsyslog.conf
may not be perfect for some debugging needs, but basic debug output will
work - and that is what most often is required. There are limited
options available, but these cover the most important use cases.

Debug processing is done via legacy config statements. There currently
is no plan to move these over to the v6+ config system. Available
settings are

-  $DebugFile <filename> - sets the debug file name
-  $DebugLevel <0\|1\|2> - sets the respective debug level, where 0
   means debug off, 1 is debug on demand activated (but debug mode off)
   and 2 is full debug mode.

Note that in theory it is forbidden to specify these parameters more
than once. However, we do not enforce that and if it happens results are
undefined.

Getting debug information from a running Instance
-------------------------------------------------

It is possible to obtain debugging information from a running instance,
but this requires some setup. We assume that the instance runs in the
background, so debug output to stdout is not desired. As such, all debug
information needs to go into a log file.

To create this setup, you need to

-  point the RSYSLOG\_DEBUGLOG environment variable to a file that is
   accessible during the while runtime (we strongly suggest a file in
   the local file system!)
-  set RSYSLOG\_DEBUG at least to "DebugOnDemand NoStdOut"
-  make sure these environment variables are set in the correct
   (distro-specific) startup script if you do not run rsyslogd
   interactively

These settings enable the capability to react to SIGUSR1. The signal
will toggle debug status when received. So send it once to turn debug
logging on, and send it again to turn debug logging off again. The third
time it will be turned on again ... and so on.

On a typical system, you can signal rsyslogd as follows:

::

    kill -USR1 $(cat /var/run/rsyslogd.pid)

The debug log will show whether debug
logging has been turned on or off. There is no other indication of the
status.

Note: running with DebugOnDemand by itself does in practice not have any
performance toll. However, switching debug logging on has a severe
performance toll. Also, debug logging synchronizes much of the code,
removing a lot of concurrency and thus potential race conditions. As
such, the very same running instance may behave very differently with
debug logging turned on vs. off. The on-demand debug log functionality
is considered to be very valuable to analyze hard-to-find bugs that only
manifest after a long runtime. Turning debug logging on a failing
instance may reveal the cause of the failure. However, depending on the
failure, debug logging may not even be successfully turned on. Also
note that with this rsyslog version we cannot obtain any debug
information on events that happened *before* debug logging was turned
on.


Interpreting the Logs
---------------------

Debug logs are primarily meant for rsyslog developers. But they may
still provide valuable information to users. Just be warned that logs
sometimes contains information the looks like an error, but actually is
none. We put a lot of extra information into the logs, and there are
some cases where it is OK for an error to happen, we just wanted to
record it inside the log. The code handles many cases automatically. So,
in short, the log may not make sense to you, but it (hopefully) makes
sense to a developer. Note that we developers often need many lines of
the log file, it is relatively rare that a problem can be diagnosed by
looking at just a couple of (hundred) log records.

Security Risks
--------------

The debug log will reveal potentially sensible information, including
user accounts and passwords, to anyone able to read the log file. As
such, it is recommended to properly guard access to the log file. Also,
an instance running with debug log enabled runs much slower than one
without. An attacker may use this to place carry out a denial-of-service
attack or try to hide some information from the log file. As such, it is
suggested to enable DebugOnDemand mode only for a reason. Note that when
no debug mode is enabled, SIGUSR1 is completely ignored.

When running in any of the debug modes (including on demand mode), an
interactive instance of rsyslogd can be aborted by pressing ctl-c.

See Also
--------

-  `How to use debug on
   demand <http://www.rsyslog.com/how-to-use-debug-on-demand/>`_
- :doc:`Quick debug guide<../troubleshooting/howtodebug>`

