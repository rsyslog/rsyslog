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

There is some additional information available in the `rsyslog
wiki <http://wiki.rsyslog.com/index.php/Security#Dropping_Privileges>`_.

**Configuration Directives**:

-  **$PrivDropToUser**
   Name of the user rsyslog should run under after startup. Please note
   that this user is looked up in the system tables. If the lookup
   fails, privileges are NOT dropped. Thus it is advisable to use the
   less convenient $PrivDropToUserID directive. If the user id can be
   looked up, but can not be set, rsyslog aborts.
-  **$PrivDropToUserID**
   Much the same as $PrivDropToUser, except that a numerical user id
   instead of a name is specified.Thus, privilege drop will always
   happen. rsyslogd aborts.
-  **$PrivDropToGroup**
   Name of the group rsyslog should run under after startup. Please
   note that this user is looked up in the system tables. If the lookup
   fails, privileges are NOT dropped. Thus it is advisable to use the
   less convenient $PrivDropToGroupID directive. Note that all
   supplementary groups are removed from the process if $PrivDropToGroup
   is specified. If the group id can be looked up, but can not be set,
   rsyslog aborts.
-  **$PrivDropToGroupID**
   Much the same as $PrivDropToGroup, except that a numerical group id
   instead of a name is specified. Thus, privilege drop will always
   happen.

