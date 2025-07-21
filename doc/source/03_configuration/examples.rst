Examples
--------

Below are example for templates and selector lines. I hope they are
self-explanatory.

Templates
~~~~~~~~~

Please note that the samples are split across multiple lines. A template
MUST NOT actually be split across multiple lines.

A template that resembles traditional syslogd file output:
 $template TraditionalFormat,"%timegenerated% %HOSTNAME%
 %syslogtag%%msg:::drop-last-lf%\\n"

A template that tells you a little more about the message:
 $template precise,"%syslogpriority%,%syslogfacility%,%timegenerated%,%HOSTNAME%,
 %syslogtag%,%msg%\\n"

A template for RFC 3164 format:
 $template RFC3164fmt,"<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag%%msg%"

A template for the format traditionally used for user messages:
 $template usermsg," XXXX%syslogtag%%msg%\\n\\r"

And a template with the traditional wall-message format:
 $template wallmsg,"\\r\\n\\7Message from syslogd@%HOSTNAME% at %timegenerated%
 
A template that can be used for the database write (please note the SQL template option)
 $template MySQLInsert,"insert iut, message, received at values
 ('%iut%', '%msg:::UPPERCASE%', '%timegenerated:::date-mysql%')
 into systemevents\\r\\n", SQL

The following template emulates
`WinSyslog <http://www.winsyslog.com/en/>`_ format (it's an
`Adiscon <http://www.adiscon.com/>`_ format, you do not feel bad if
you don't know it ;)). It's interesting to see how it takes different
parts out of the date stamps. What happens is that the date stamp is
split into the actual date and time and the these two are combined with
just a comma in between them.

::

 $template WinSyslogFmt,"%HOSTNAME%,%timegenerated:1:10:date-rfc3339%,
 %timegenerated:12:19:date-rfc3339%,%timegenerated:1:10:date-rfc3339%,
 %timegenerated:12:19:date-rfc3339%,%syslogfacility%,%syslogpriority%,
 %syslogtag%%msg%\\n"

Selector lines
~~~~~~~~~~~~~~

::

  # Store critical stuff in critical
  #
  *.=crit;kern.none /var/adm/critical

This will store all messages with the priority crit in the file
/var/adm/critical, except for any kernel message.

::

  # Kernel messages are first, stored in the kernel
  # file, critical messages and higher ones also go
  # to another host and to the console. Messages to
  # the host server.example.net are forwarded in RFC 3164
  # format (using the template defined above).
  #
  kern.* /var/adm/kernel
  kern.crit @server.example.net;RFC3164fmt
  kern.crit /dev/console
  kern.info;kern.!err /var/adm/kernel-info

The first rule direct any message that has the kernel facility to the
file /var/adm/kernel.

The second statement directs all kernel messages of the priority crit
and higher to the remote host server.example.net. This is useful, because if the
host crashes and the disks get irreparable errors you might not be able
to read the stored messages. If they're on a remote host, too, you still
can try to find out the reason for the crash.

The third rule directs these messages to the actual console, so the
person who works on the machine will get them, too.

The fourth line tells rsyslogd to save all kernel messages that come
with priorities from info up to warning in the file /var/adm/kernel-info. 
Everything from err and higher is excluded.

::

  # The tcp wrapper loggs with mail.info, we display
  # all the connections on tty12
  #
  mail.=info /dev/tty12

This directs all messages that uses mail.info (in source LOG\_MAIL \|
LOG\_INFO) to /dev/tty12, the 12th console. For example the tcpwrapper
tcpd(8) uses this as it's default.

::

  # Store all mail concerning stuff in a file
  #
  mail.\*;mail.!=info /var/adm/mail

This pattern matches all messages that come with the mail facility,
except for the info priority. These will be stored in the file
/var/adm/mail.

::

  # Log all mail.info and news.info messages to info
  #
  mail,news.=info /var/adm/info

This will extract all messages that come either with mail.info or with
news.info and store them in the file /var/adm/info.

::

  # Log info and notice messages to messages file
  #
  *.=info;*.=notice;\
  mail.none /var/log/messages

This lets rsyslogd log all messages that come with either the info or
the notice facility into the file /var/log/messages, except for all
messages that use the mail facility.

::

  # Log info messages to messages file
  #
  *.=info;\
  mail,news.none /var/log/messages

This statement causes rsyslogd to log all messages that come with the
info priority to the file /var/log/messages. But any message coming
either with the mail or the news facility will not be stored.

::

  # Emergency messages will be displayed to all users
  #
  *.=emerg :omusrmsg:*

This rule tells rsyslogd to write all emergency messages to all
currently logged in users.

::

  # Messages of the priority alert will be directed
  # to the operator
  #
  *.alert root,rgerhards

This rule directs all messages with a priority of alert or higher to
the terminals of the operator, i.e. of the users "root'' and
"rgerhards'' if they're logged in.

::

  *.* @server.example.net

This rule would redirect all messages to a remote host called
server.example.net. This is useful especially in a cluster of machines where all
syslog messages will be stored on only one machine.

In the format shown above, UDP is used for transmitting the message.
The destination port is set to the default auf 514. Rsyslog is also
capable of using much more secure and reliable TCP sessions for message
forwarding. Also, the destination port can be specified. To select TCP,
simply add one additional @ in front of the host name (that is, @host is
UDP, @@host is TCP). For example:

::

  *.* @@server.example.net

To specify the destination port on the remote machine, use a colon
followed by the port number after the machine name. The following
forwards to port 1514 on server.example.net:

::

  *.* @@server.example.net:1514

This syntax works both with TCP and UDP based syslog. However, you will
probably primarily need it for TCP, as there is no well-accepted port
for this transport (it is non-standard). For UDP, you can usually stick
with the default auf 514, but might want to modify it for security reasons.
If you would like to do that, it's quite easy:

::

  *.* @server.example.net:1514
  *.* >dbhost,dbname,dbuser,dbpassword;dbtemplate

This rule writes all message to the database "dbname" hosted on
"dbhost". The login is done with user "dbuser" and password
"dbpassword". The actual table that is updated is specified within the
template (which contains the insert statement). The template is called
"dbtemplate" in this case.

::

  :msg,contains,"error" @server.example.net

This rule forwards all messages that contain the word "error" in the msg
part to the server "errorServer". Forwarding is via UDP. Please note the
colon in front

