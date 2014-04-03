Mail Output Module (ommail)
===========================

**Module Name:    ommail**

**Available since:   ** 3.17.0

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module supports sending syslog messages via mail. Each syslog
message is sent via its own mail. Obviously, you will want to apply
rigorous filtering, otherwise your mailbox (and mail server) will be
heavily spammed. The ommail plugin is primarily meant for alerting
users. As such, it is assume that mails will only be sent in an
extremely limited number of cases.

Please note that ommail is especially well-suited to work in tandem with
`imfile <imfile.html>`_ to watch files for the occurence of specific
things to be alerted on. So its scope is far broader than forwarding
syslog messages to mail recipients.

Ommail uses two templates, one for the mail body and one for the subject
line. If neither is provided, a quite meaningless subject line is used
and the mail body will be a syslog message just as if it were written to
a file. It is expected that the users customizes both messages. In an
effort to support cell phones (including SMS gateways), there is an
option to turn off the body part at all. This is considered to be useful
to send a short alert to a pager-like device.
 It is highly recommended to use the  "$ActionExecOnlyOnceEveryInterval
<seconds>" directive to limit the amount of mails that potentially be
generated. With it, mails are sent at most in a <seconds> interval. This
may be your life safer. And remember that an hour has 3,600 seconds, so
if you would like to receive mails at most once every two hours, include
a "$ActionExecOnlyOnceEveryInterval 7200" immediately before the ommail
action. Messages sent more frequently are simpy discarded.

**Configuration Directives**:

-  $ActionMailSMTPServer
    Name or IP address of the SMTP server to be used. Must currently be
   set. The default is 127.0.0.1, the SMTP server on the local machine.
   Obviously it is not good to expect one to be present on each machine,
   so this value should be specified.
-  $ActionMailSMTPPort
    Port number or name of the SMTP port to be used. The default is 25,
   the standard SMTP port.
-  $ActionMailFrom
    The email address used as the senders address. There is no default.
-  $ActionMailTo
    The recipient email addresses. There is no default. To specify
   multiple recpients, repeat this directive as often as needed. Note:
   **This directive must be specified for each new action and is
   automatically reset.** [Multiple recipients are supported for 3.21.2
   and above.]
-  $ActionMailSubject
    The name of the template to be used as the mail subject. If this is
   not specified, a more or less meaningless mail subject is generated
   (we don't tell you the exact text because that can change - if you
   want to have something specific, configure it!).
-  $ActionMailEnableBody
    Setting this to "off" permits to exclude the actual message body.
   This may be useful for pager-like devices or cell phone SMS messages.
   The default is "on", which is appropriate for allmost all cases. Turn
   it off only if you know exactly what you do!

**Caveats/Known Bugs:**

The current ommail implementation supports SMTP-direct mode only. In
that mode, the plugin talks to the mail server via SMTP protocol. No
other process is involved. This mode offers best reliability as it is
not depending on any external entity except the mail server. Mail server
downtime is acceptable if the action is put onto its own action queue,
so that it may wait for the SMTP server to come back online. However,
the module implements only the bare SMTP essentials. Most importantly,
it does not provide any authentication capabilities. So your mail server
must be configured to accept incoming mail from ommail without any
authentication needs (this may be change in the future as need arises,
but you may also be referred to sendmail-mode).

In theory, ommail should also offer a mode where it uses the sendmail
utility to send its mail (sendmail-mode). This is somewhat less reliable
(because we depend on an entity we do not have close control over -
sendmail). It also requires dramatically more system ressources, as we
need to load the external process (but that should be no problem given
the expected infrequent number of calls into this plugin). The big
advantage of sendmail mode is that it supports all the bells and
whistles of a full-blown SMTP implementation and may even work for local
delivery without a SMTP server being present. Sendmail mode will be
implemented as need arises. So if you need it, please drop us a line (I
nobody does, sendmail mode will probably never be implemented).

**Sample:**

The following sample alerts the operator if the string "hard disk fatal
failure" is present inside a syslog message. The mail server at
mail.example.net is used and the subject shall be "disk problem on
<hostname>". Note how \\r\\n is included inside the body text to create
line breaks. A message is sent at most once every 6 hours, any other
messages are silently discarded (or, to be precise, not being forwarded
- they are still being processed by the rest of the configuration file).

$ModLoad ommail $ActionMailSMTPServer mail.example.net $ActionMailFrom
rsyslog@example.net $ActionMailTo operator@example.net $template
mailSubject,"disk problem on %hostname%" $template mailBody,"RSYSLOG
Alert\\r\\nmsg='%msg%'" $ActionMailSubject mailSubject # make sure we
receive a mail only once in six # hours (21,600 seconds ;))
$ActionExecOnlyOnceEveryInterval 21600 # the if ... then ... mailBody
mus be on one line! if $msg contains 'hard disk fatal failure' then
:ommail:;mailBody

The sample below is the same, but sends mail to two recipients:

$ModLoad ommail $ActionMailSMTPServer mail.example.net $ActionMailFrom
rsyslog@example.net $ActionMailTo operator@example.net $ActionMailTo
admin@example.net $template mailSubject,"disk problem on %hostname%"
$template mailBody,"RSYSLOG Alert\\r\\nmsg='%msg%'" $ActionMailSubject
mailSubject # make sure we receive a mail only once in six # hours
(21,600 seconds ;)) $ActionExecOnlyOnceEveryInterval 21600 # the if ...
then ... mailBody mus be on one line! if $msg contains 'hard disk fatal
failure' then :ommail:;mailBody

A more advanced example plus a discussion on using the email feature
inside a reliable system can be found in Rainer's blogpost "`Why is
native email capability an advantage for a
syslogd? <http://rgerhards.blogspot.com/2008/04/why-is-native-email-capability.html>`_\ "

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
