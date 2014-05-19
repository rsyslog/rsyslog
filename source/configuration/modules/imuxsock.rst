imuxsock: Unix Socket Input
===========================

**Module Name:    imuxsock**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

**Provides the ability to accept syslog messages via local Unix sockets.
Most importantly, this is the mechanism by which the syslog(3) call
delivers syslog messages to rsyslogd.** So you need to have this module
loaded to read the system log socket and be able to process log messages
from applications running on the local system.

**Application-provided timestamps are ignored by default.** This is
needed, as some programs (e.g. sshd) log with inconsistent timezone
information, what messes up the local logs (which by default don't even
contain time zone information). This seems to be consistent with what
sysklogd did for the past four years. Alternate behaviour may be
desirable if gateway-like processes send messages via the local log slot
- in this case, it can be enabled via the
$InputUnixListenSocketIgnoreMsgTimestamp and
$SystemLogSocketIgnoreMsgTimestamp config directives

**There is input rate limiting available,** (since 5.7.1) to guard you
against the problems of a wild running logging process. If more than
$SystemLogRateLimitInterval \* $SystemLogRateLimitBurst log messages are
emitted from the same process, those messages with
$SystemLogRateLimitSeverity or lower will be dropped. It is not possible
to recover anything about these messages, but imuxsock will tell you how
many it has dropped one the interval has expired AND the next message is
logged. Rate-limiting depends on SCM\_CREDENTIALS. If the platform does
not support this socket option, rate limiting is turned off. If multiple
sockets are configured, rate limiting works independently on each of
them (that should be what you usually expect). The same functionality is
available for additional log sockets, in which case the config
statements just use the prefix $IMUXSockRateLimit... but otherwise works
exactly the same. When working with severities, please keep in mind that
higher severity numbers mean lower severity and configure things
accordingly. To turn off rate limiting, set the interval to zero.

**Unix log sockets can be flow-controlled.** That is, if processing
queues fill up, the unix socket reader is blocked for a short while.
This may be useful to prevent overruning the queues (which may cause
exessive disk-io where it actually would not be needed). However,
flow-controlling a log socket (and especially the system log socket) can
lead to a very unresponsive system. As such, flow control is disabled by
default. That means any log records are places as quickly as possible
into the processing queues. If you would like to have flow control, you
need to enable it via the $SystemLogSocketFlowControl and
$InputUnixListenSocketFlowControl config directives. Just make sure you
thought about the implications. Note that for many systems, turning on
flow control does not hurt.

Starting with rsyslog 5.9.4, **`trusted syslog
properties <http://www.rsyslog.com/what-are-trusted-properties/>`_ are
available**. These require a recent enough Linux Kernel and access to
the /proc file system. In other words, this may not work on all
platforms and may not work fully when privileges are dropped (depending
on how they are dropped). Note that trusted properties can be very
useful, but also typically cause the message to grow rather large. Also,
the format of log messages is obviously changed by adding the trusted
properties at the end. For these reasons, the feature is **not enabled
by default**. If you want to use it, you must turn it on (via
$SystemLogSocketAnnotate and $InputUnixListenSocketAnnotate).

**Configuration Directives**:

-  **$InputUnixListenSocketIgnoreMsgTimestamp** [**on**/off]
   Ignore timestamps included in the message. Applies to the next socket
   being added.
-  **$InputUnixListenSocketFlowControl** [on/**off**] - specifies if
   flow control should be applied to the next socket.
-  **$IMUXSockRateLimitInterval** [number] - specifies the rate-limiting
   interval in seconds. Default value is 0, which turns off rate
   limiting. Set it to a number of seconds (5 recommended) to activate
   rate-limiting. The default of 0 has been choosen in 5.9.6+, as people
   experienced problems with this feature activated by default. Now it
   needs an explicit opt-in by setting this parameter.
-  **$IMUXSockRateLimitBurst** [number] - specifies the rate-limiting
   burst in number of messages. Default is 200.
-  **$IMUXSockRateLimitSeverity** [numerical severity] - specifies the
   severity of messages that shall be rate-limited.
-  **$IMUXSockLocalIPIF** [interface name] - (available since 5.9.6) -
   if provided, the IP of the specified interface (e.g. "eth0") shall be
   used as fromhost-ip for imuxsock-originating messages. If this
   directive is not given OR the interface cannot be found (or has no IP
   address), the default of "127.0.0.1" is used.
-  **$InputUnixListenSocketUsePIDFromSystem** [on/**off**] - specifies
   if the pid being logged shall be obtained from the log socket itself.
   If so, the TAG part of the message is rewritten. It is recommended to
   turn this option on, but the default is "off" to keep compatible with
   earlier versions of rsyslog. This option was introduced in 5.7.0.
-  **$InputUnixListenSocketUseSysTimeStamp** [**on**/off] instructs
   imuxsock to obtain message time from the system (via control
   messages) insted of using time recorded inside the message. This may
   be most useful in combination with systemd. Note: this option was
   introduced with version 5.9.1. Due to the usefulness of it, we
   decided to enable it by default. As such, 5.9.1 and above behave
   slightly different than previous versions. However, we do not see how
   this could negatively affect existing environments.
-  **$SystemLogSocketIgnoreMsgTimestamp** [**on**/off]
   Ignore timestamps included in the messages, applies to messages
   received via the system log socket.
-  **$OmitLocalLogging** (imuxsock) [on/**off**] -- former -o option; do
   NOT listen for the local log socket. This is most useful if you run
   multiple instances of rsyslogd where only one shall handle the system
   log socket.
-  **$SystemLogSocketName** <name-of-socket> -- former -p option
-  **$SystemLogFlowControl** [on/**off**] - specifies if flow control
   should be applied to the system log socket.
-  **$SystemLogUsePIDFromSystem** [on/**off**] - specifies if the pid
   being logged shall be obtained from the log socket itself. If so, the
   TAG part of the message is rewritten. It is recommended to turn this
   option on, but the default is "off" to keep compatible with earlier
   versions of rsyslog. This option was introduced in 5.7.0.
-  **$SystemLogRateLimitInterval** [number] - specifies the
   rate-limiting interval in seconds. Default value is 5 seconds. Set it
   to 0 to turn rate limiting off.
-  **$SystemLogRateLimitBurst** [number] - specifies the rate-limiting
   burst in number of messages. Default is 200.
-  **$SystemLogRateLimitSeverity** [numerical severity] - specifies the
   severity of messages that shall be rate-limited.
-  **$SystemLogUseSysTimeStamp** [**on**/off] the same as
   $InputUnixListenSocketUseSysTimeStamp, but for the system log socket.
-  **$InputUnixListenSocketCreatePath** [on/**off**] - create
   directories in the socket path if they do not already exist. They are
   created with 0755 permissions with the owner being the process under
   which rsyslogd runs. The default is not to create directories. Keep
   in mind, though, that rsyslogd always creates the socket itself if it
   does not exist (just not the directories by default).
   Note that this statement affects the next $AddUnixListenSocket
   directive that follows in sequence in the configuration file. It
   never works on the system log socket (where it is deemed
   unnecessary). Also note that it is automatically being reset to "off"
   after the $AddUnixListenSocket directive, so if you would have it
   active for two additional listen sockets, you need to specify it in
   front of each one. This option is primarily considered useful for
   defining additional sockets that reside on non-permanent file
   systems. As rsyslogd probably starts up before the daemons that
   create these sockets, it is a vehicle to enable rsyslogd to listen to
   those sockets even though their directories do not yet exist.
   [available since 4.7.0 and 5.3.0]
-  **$AddUnixListenSocket** <name-of-socket> adds additional unix
   socket, default none -- former -a option
-  **$InputUnixListenSocketHostName** <hostname> permits to override the
   hostname that shall be used inside messages taken from the **next**
   $AddUnixListenSocket socket. Note that the hostname must be specified
   before the $AddUnixListenSocket configuration directive, and it will
   only affect the next one and then automatically be reset. This
   functionality is provided so that the local hostname can be
   overridden in cases where that is desired.
-  **$InputUnixListenSocketAnnotate** <on/**off**> turn on
   annotation/trusted properties for the non-system log socket in
   question.
-  **$SystemLogSocketAnnotate** <on/**off**> turn on annotation/trusted
   properties for the system log socket.

**Caveats/Known Bugs:**

-  There is a compile-time limit of 50 concurrent sockets. If you need
   more, you need to change the array size in imuxsock.c.
-  This documentation is sparse and incomplete.

**Sample:**

The following sample is the minimum setup required to accept syslog
messages from applications running on the local system.

$ModLoad imuxsock # needs to be done just once
$SystemLogSocketFlowControl on # enable flow control (use if needed)

The following sample is a configuration where rsyslogd pulls logs from
two jails, and assigns different hostnames to each of the jails:

$ModLoad imuxsock # needs to be done just once
$InputUnixListenSocketHostName jail1.example.net $AddUnixListenSocket
/jail/1/dev/log $InputUnixListenSocketHostName jail2.example.net
$AddUnixListenSocket /jail/2/dev/log

The following sample is a configuration where rsyslogd reads the openssh
log messages via a separate socket, but this socket is created on a
temporary file system. As rsyslogd starts up before the sshd, it needs
to create the socket directories, because it otherwise can not open the
socket and thus not listen to openssh messages. Note that it is vital
not to place any other socket between the
$InputUnixListenSocketCreatePath and the $InputUnixListenSocketHostName.

$ModLoad imuxsock # needs to be done just once
$InputUnixListenSocketCreatePath on # turn on for \*next\* socket
$InputUnixListenSocketHostName /var/run/sshd/dev/log

The following sample is used to turn off input rate limiting on the
system log socket. $ModLoad imuxsock # needs to be done just once
$SystemLogRateLimitInterval 0 # turn off rate limiting

The following sample is used activate message annotation and thus
trusted properties on the system log socket. $ModLoad imuxsock # needs
to be done just once $SystemLogSocketAnnotate on

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2008-2014 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
