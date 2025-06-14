Dropping privileges in rsyslog
==============================

**Available since**: 4.1.1

**Description**:

Rsyslogd provides the ability to drop privileges by impersonating as
another user and/or group after startup.

Please note that due to POSIX standards, rsyslogd always needs to start
up as root if there is a listener who must bind to a network port below
1024. For example, the UDP listener usually needs to listen to 514 and
therefore rsyslogd needs to start up as root.

If you do not need this functionality, you can start rsyslog directly as
an ordinary user. That is probably the safest way of operations.
However, if a startup as root is required, you can use the
$PrivDropToGroup and $PrivDropToUser config directives to specify a
group and/or user that rsyslogd should drop to after initialization.
Once this happens, the daemon runs without high privileges (depending,
of course, on the permissions of the user account you specified).

A special note for Docker and other container system users: user and
group names are usually not fully mirrored into containers. As such,
we strongly advise to use numerical IDs instead of user or group
names when configuring privilege drop.

Privilege drop is configured via the
:doc:`global configuration object<../rainerscript/global>` under the
"`privilege.`" set of parameters.
