$omfileForceChown
-----------------

**Type:** action configuration parameter

**Parameter Values:** boolean (on/off, yes/no)

**Available:** 4.7.0+, 5.3.0-5.8.x, **NOT** available in 5.9.x or higher

**Note: this parameter has been removed and is no longer available. The
documentation is currently being retained for historical reasons.**
Expect it to go away at some later stage as well.

**Default:** off

**Description:**

Forces rsyslogd to change the ownership for output files that already
exist. Please note that this tries to fix a potential problem that
exists outside the scope of rsyslog. Actually, it tries to fix invalid
ownership/permission settings set by the original file creator.

Rsyslog changes the ownership during initial execution with root
privileges. When a privilege drop is configured, privileges are dropped
after the file owner ship is changed. Not that this currently is a
limitation in rsyslog's privilege drop code, which is on the TODO list
to be removed. See Caveats section below for the important implications.

**Caveats:**

This parameter tries to fix a problem that actually is outside the scope
of rsyslog. As such, there are a couple of restrictions and situations
in which it will not work. **Users are strongly encouraged to fix their
system instead of turning this parameter on** - it should only be used
as a last resort.

At least in the following scenario, this parameter will fail expectedly:

It does not address the situation that someone changes the ownership
\*after\* rsyslogd has started. Let's, for example, consider a log
rotation script.

-  rsyslog is started
-  ownership is changed
-  privileges dropped
-  log rotation (lr) script starts
-  lr removes files
-  lr creates new files with root:adm (or whatever else)
-  lr HUPs rsyslogd
-  rsyslogd closes files
-  rsyslogd tries to open files
-  rsyslogd tries to change ownership --> fail as we are non-root now
-  file open fails

Please note that once the privilege drop code is refactored, this
parameter will no longer work, because then privileges will be dropped
before any action is performed, and thus we will no longer be able to
chown files that do not belong to the user rsyslogd is configured to run
under.

So **expect the parameter to go away**. It will not be removed in
version 4, but may disappear at any time for any version greater than 4.

**Sample:**

``$FileOwner loguser $omfileForceChown on``

