
***************
sysklogd format
***************

This is the format in use since the beginning of syslogging. It still
is an excellent choice to do very simple things.

For more advanced things, use the |FmtAdvancedName| format.

DESCRIPTION
===========

The syslog.conf file is the main configuration file for :manpage:`syslogd(8)`
which logs system messages on \*nix systems. This file specifies rules for
logging. For special features see the sysklogd(8) manpage.

Every rule consists of two fields, a selector field and an action field.
These two fields are separated by one or more spaces or tabs. The selector
field specifies a pattern of facilities and priorities belonging to the
specified action.

Lines starting with a hash mark ("#") and empty lines are ignored.

This variant of syslogd is able to understand a slightly extended syntax
compared to the original BSD syslogd. One rule may be divided into several
lines if the leading line is terminated with a backslash ("\\").

SELECTORS
=========

The selector field consists of two parts, a facility and a priority, separated
by a period ("."). Both parts are case insensitive and can also be specified
as decimal numbers corresponding to the definitions in
``/usr/include/syslog.h``. It is safer to use symbolic names rather than
decimal numbers. Both facilities and priorities are described in
:manpage:`syslog(3)`. The names mentioned below
correspond to the similar ``LOG_`` values in ``/usr/include/syslog.h``.

The facility is one of the following keywords: auth, authpriv, cron, daemon,
ftp, kern, lpr, mail, mark, news, security (same as auth), syslog, user, uucp
and local0 through local7. The keyword security is deprecated and mark is only
for internal use and therefore should not be used in applications. The facility
specifies the subsystem that produced the message, e.g. all mail programs log
with the mail facility (LOG_MAIL) if they log using syslog.

In most cases anyone can log to any facility, so we rely on convention for the
correct facility to be chosen. However, generally only the kernel can log to
the "kern" facility. This is because the implementation of ``openlog()`` and
``syslog()`` in glibc does not allow logging to the "kern" facility. Klogd
circumvents this restriction when logging to syslogd by reimplementing those
functions itself.

The priority is one of the following keywords, in ascending order: debug,
info, notice, warning, warn (same as warning), err, error (same as err), crit,
alert, emerg, panic (same as emerg). The keywords warn, error and panic are
deprecated and should not be used anymore. The priority defines the severity of
the message.

The behavior of the original BSD syslogd is that all messages of the specified
priority and higher are logged according to the given action. This
:manpage:`syslogd(8)` behaves the same, but has some extensions.

In addition to the above mentioned names the :manpage:`syslogd(8)` understands
the following extensions:
An asterisk ("\*") stands for all facilities or all priorities, depending on
where it is used (before or after the period). The keyword none stands for no
priority of the given facility.

Multiple facilities may be specified for a single priority pattern in one
statement using the comma (",") operator to separate the facilities. You may
specify as many facilities as you want. Please note that only the facility
part from such a statement is taken, a priority part would be ignored.

Multiple selectors may be specified for a single action using the semicolon
(";") separator. Selectors are processed from left to right, with each selector
being able to overwrite preceding ones. Using this behavior you are able to
exclude some priorities from the pattern.

This :manpage:`syslogd(8)` has a syntax extension to the original BSD source,
which makes its use more intuitive. You may precede every priority with an
equation sign ("=") to specify that syslogd should only refer to this single
priority and not this priority and all higher priorities.

You may also precede the priority with an exclamation mark ("!") if you want
syslogd to ignore this priority and all higher priorities. You may even use
both, the exclamation mark and the equation sign if you want syslogd to ignore
only this single priority. If you use both extensions then the exclamation
mark must occur before the equation sign, just use it intuitively.

ACTIONS
=======

The action field of a rule describes the abstract term "logfile". A "logfile"
need not to be a real file, btw. The :manpage:`syslogd(8)` provides the
following actions.

Regular File
------------

Typically messages are logged to real files. The filename is specified with an
absolute pathname. It may be specified as a file name relative to rsyslog's
working directory if the filename starts with "." or "..". However, this is
dangerous and should be avoided.

Named Pipes
-----------

This version of :manpage:`syslogd(8)` has support for logging output to named
pipes (fifos). A fifo or named pipe can be used as a destination for log
messages by prepending a pipe symbol ("|") to the name of the file. This is
handy for debugging. Note that the fifo must be created with the
:manpage:`mkfifo(1)` command before :manpage:`syslogd(8)` is started.

Terminal and Console
--------------------

If the file you specified is a tty, special tty-handling is done, same with
``/dev/console``.

Remote Machine
--------------

This :manpage:`syslogd(8)` provides full remote logging, i.e. is able to send
messages to a remote host running :manpage:`syslogd(8)` and to receive messages
from remote hosts. The remote host won't forward the message again, it will
just log them locally. To forward messages to another host, prepend the
hostname with the at sign ("@").

Using this feature you are able to collect all syslog messages on a central
host, if all other machines log remotely to that one. This reduces
administration needs.

Using a named pipe log method, messages from remote hosts can be sent to a
log program. By reading log messages line by line such a program is able to
sort log messages by host name or program name on the central log host. This
way it is possible to split the log into separate files.

List of Users
-------------

Usually critical messages are also directed to "root" on that machine. You can
specify a list of users that ought to receive the log message on the terminal
by writing their usernames. You may specify more than one user by separating
the usernames with commas (","). If they're logged in they will receive the
log messages.

Everyone logged on
------------------

Emergency messages often go to all users currently online to notify them that
something strange is happening with the system. To specify this wall(1)-feature
use an asterisk ("*").

EXAMPLES
========

Here are some examples, partially taken from a real existing site and
configuration. Hopefully they answer all questions about configuring this
:manpage:`syslogd(8)`. If not, don't hesitate to contact the mailing list.

::

  # Store critical stuff in critical
  #
  *.=crit;kern.none   /var/adm/critical

This will store all messages of priority crit in the file
``/var/adm/critical``, with the exception of any kernel messages.

::

   # Kernel messages are stored in the kernel file,
   # critical messages and higher ones also go
   # to another host and to the console
   #
   kern.*      /var/adm/kernel
   kern.crit     @finlandia
   kern.crit     /dev/console
   kern.info;kern.!err   /var/adm/kernel-info

The first rule directs any message that has the kernel facility to the file
``/var/adm/kernel``. (But recall that only the kernel itself can log to this
facility.)

The second statement directs all kernel messages of priority crit and higher
to the remote host finlandia. This is useful, because if the host crashes
and the disks get irreparable errors you might not be able to read the stored
messages. If they're on a remote host, too, you still can try to find out the
reason for the crash.

The third rule directs kernel messages of priority crit and higher to the
actual console, so the person who works on the machine will get them, too.

The fourth line tells the syslogd to save all kernel messages that come with
priorities from info up to warning in the file ``/var/adm/kernel-info``.

This is an example of the 2nd selector overwriting part of the first one.
The first selector selects kernel messages of priority info and higher. The
second selector filters out kernel messages of priority error and higher.
This leaves just priorities info, notice and warning to get logged.

::

  # The tcp wrapper logs with mail.info, we display
  # all the connections on tty12
  #
  mail.=info     /dev/tty12

This directs all messages that use ``mail.info``
(in source ``LOG_MAIL | LOG_INFO``) to ``/dev/tty12``, the 12th console.
For example the tcpwrapper :manpage:`tcpd(8)` uses this as its default.

::

  # Write all mail related logs to a file
  #
  mail.*;mail.!=info   /var/adm/mail

This pattern matches all messages that come with the mail facility,
except for the info priority. These will be stored in the file
``/var/adm/mail``.

::

  # Log all mail.info and news.info messages to info
  #
  mail,news.=info    /var/adm/info

This will extract all messages that come either with mail.info or with
news.info and store them in the file ``/var/adm/info``.

::

  # Log info and notice messages to messages file
  #
  *.=info;*.=notice;\
  mail.none /var/log/messages

This lets the syslogd log all messages that come with either the info or the
notice priority into the file ``/var/log/messages``, except for all messages
that use the mail facility.

::

  # Log info messages to messages file
  #
  *.=info;\
  mail,news.none  /var/log/messages

This statement causes the syslogd to log all messages that come with the info
priority to the file ``/var/log/messages``. But any message coming either with
the mail or the news facility will not be stored.

::

  # Emergency messages will be displayed using wall
  #
  *.=emerg      *

This rule tells the syslogd to write all emergency messages to all currently
logged in users. This is the wall action.

::

  # Messages of the priority alert will be directed
  # to the operator
  #
  *.alert      root,joey

This rule directs all messages of priority alert or higher to the terminals
of the operator, i.e. of the users "root" and "joey" if they're logged in.

::

  *.*       @finlandia

This rule would redirect all messages to a remote host called finlandia.
This is useful especially in a cluster of machines where all syslog messages
will be stored on only one machine.

CONFIGURATION FILE SYNTAX DIFFERENCES
=====================================

Syslogd uses a slightly different syntax for its configuration file than the
original BSD sources. Originally all messages of a specific priority and above
were forwarded to the log file. The modifiers "=", "!" and "-" were added to
make the syslogd more flexible and to use it in a more intuitive manner.

The original BSD syslogd doesn't understand spaces as separators between the
selector and the action field.

BUGS
====

The effects of multiple selectors are sometimes not intuitive. For example
"mail.crit,\*.err" will select "mail" facility messages at the level of
"err" or higher, not at the level of "crit" or higher.

Also, if you specify a selector with an exclamation mark in it which is not
preceded by a corresponding selector without an exclamation mark, nothing
will be logged. Intuitively, the selector "ftp.!alert" on its own will select
all ftp messages with priorities less than alert. In fact it selects nothing.
Similarly "ftp.!=alert" might reasonably be expected to select all ftp messages
other than those with priority alert, but again it selects nothing. It seems
the selectors with exclamation marks in them should only be used as "filters"
following selectors without exclamation marks.

Finally, using a backslash to divide a line into two doesn't work if the
backslash is used immediately after the end of the selector, without
intermediate whitespace.
