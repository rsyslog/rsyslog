**************************
ommail: Mail Output Module
**************************

.. index:: ! imudp

===========================  ===========================================================================
**Module Name:**             **ommail**
**Available Since:**         **3.17.0**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module supports sending syslog messages via mail. Each syslog
message is sent via its own mail. Obviously, you will want to apply
rigorous filtering, otherwise your mailbox (and mail server) will be
heavily spammed. The ommail plugin is primarily meant for alerting
users. As such, it is assumed that mails will only be sent in an
extremely limited number of cases.

Ommail uses up to two templates, one for the mail body and optionally
one for the subject line. Note that the subject line can also be set to
a constant text.
If neither the subject nor the mail body is provided, a quite meaningless
subject line is used
and the mail body will be a syslog message just as if it were written to
a file. It is expected that the users customizes both messages. In an
effort to support cell phones (including SMS gateways), there is an
option to turn off the body part at all. This is considered to be useful
to send a short alert to a pager-like device.
It is highly recommended to use the 

.. code-block:: none

  action.execonlyonceeveryinterval="<seconds>"

parameter to limit the amount of mails that potentially be
generated. With it, mails are sent at most in a <seconds> interval. This
may be your life safer. And remember that an hour has 3,600 seconds, so
if you would like to receive mails at most once every two hours, include
a

.. code-block:: none

  action.execonlyonceeveryinterval="7200"

in the action definition. Messages sent more frequently are simply discarded.


Configuration Parameters
========================

Configuration parameters are supported starting with v8.5.0. Earlier
v7 and v8 versions did only support legacy parameters.

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "``$ActionMailSMTPServer``"

Name or IP address of the SMTP server to be used. Must currently be
set. The default is 127.0.0.1, the SMTP server on the local machine.
Obviously it is not good to expect one to be present on each machine,
so this value should be specified.


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "``$ActionMailSMTPPort``"

Port number or name of the SMTP port to be used. The default is 25,
the standard SMTP port.


MailFrom
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "``$ActionMailFrom``"

The email address used as the senders address.


MailTo
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "yes", "``$ActionMailTo``"

The recipient email address(es). Note that this is an array parameter. See
samples below on how to specify multiple recipients.


Subject.Template
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$ActionMailSubject``"

The name of the template to be used as the mail subject.

If you want to include some information from the message inside the
template, you need to use *subject.template* with an appropriate template.
If you just need a constant text, you can simply use *subject.text*
instead, which doesn't require a template definition.


Subject.Text
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

This is used to set a **constant** subject text.


Body.Enable
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$ActionMailEnableBody``"

Setting this to "off" permits to exclude the actual message body.
This may be useful for pager-like devices or cell phone SMS messages.
The default is "on", which is appropriate for almost all cases. Turn
it off only if you know exactly what you do!


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_FileFormat", "no", "none"

Template to be used for the mail body (if enabled).

The *template.subject* and *template.text* parameters cannot be given together
inside a single action definition. Use either one of them. If none is used,
a more or less meaningless mail subject is generated (we don't tell you the exact
text because that can change - if you want to have something specific, configure it!).


Caveats/Known Bugs
==================

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
sendmail). It also requires dramatically more system resources, as we
need to load the external process (but that should be no problem given
the expected infrequent number of calls into this plugin). The big
advantage of sendmail mode is that it supports all the bells and
whistles of a full-blown SMTP implementation and may even work for local
delivery without a SMTP server being present. Sendmail mode will be
implemented as need arises. So if you need it, please drop us a line (If
nobody does, sendmail mode will probably never be implemented).


Examples
========

Example 1
---------

The following example alerts the operator if the string "hard disk fatal
failure" is present inside a syslog message. The mail server at
mail.example.net is used and the subject shall be "disk problem on
<hostname>". Note how \\r\\n is included inside the body text to create
line breaks. A message is sent at most once every 6 hours (21600 seconds),
any other messages are silently discarded (or, to be precise, not being
forwarded - they are still being processed by the rest of the configuration
file).

.. code-block:: none

   module(load="ommail")

   template (name="mailBody"  type="string" string="RSYSLOG Alert\\r\\nmsg='%msg%'")
   template (name="mailSubject" type="string" string="disk problem on %hostname%")

   if $msg contains "hard disk fatal failure" then {
      action(type="ommail" server="mail.example.net" port="25"
	     mailfrom="rsyslog@example.net"
	     mailto="operator@example.net"
	     subject.template="mailSubject"
	     action.execonlyonceeveryinterval="21600")
   }


Example 2
---------

The following example is exactly like the first one, but it sends the mails
to two different email addresses:

.. code-block:: none

   module(load="ommail")

   template (name="mailBody"  type="string" string="RSYSLOG Alert\\r\\nmsg='%msg%'")
   template (name="mailSubject" type="string" string="disk problem on %hostname%")

   if $msg contains "hard disk fatal failure" then {
      action(type="ommail" server="mail.example.net" port="25"
	     mailfrom="rsyslog@example.net"
	     mailto=["operator@example.net", "admin@example.net"]
	     subject.template="mailSubject"
	     action.execonlyonceeveryinterval="21600")
   }


Example 3
---------

Note the array syntax to specify email addresses. Note that while rsyslog
permits you to specify as many recipients as you like, your mail server
may limit their number. It is usually a bad idea to use more than 50
recipients, and some servers may have lower limits. If you hit such a limit,
you could either create additional actions or (recommended) create an
email distribution list.

The next example is again mostly equivalent to the previous one, but it uses a
constant subject line, so no subject template is required:

.. code-block:: none

   module(load="ommail")

   template (name="mailBody"  type="string" string="RSYSLOG Alert\\r\\nmsg='%msg%'")

   if $msg contains "hard disk fatal failure" then {
      action(type="ommail" server="mail.example.net" port="25"
	     mailfrom="rsyslog@example.net"
	     mailto=["operator@example.net", "admin@example.net"]
	     subject.text="rsyslog detected disk problem"
	     action.execonlyonceeveryinterval="21600")
   }


Additional Resources
====================

A more advanced example plus a discussion on using the email feature
inside a reliable system can be found in Rainer's blogpost "`Why is
native email capability an advantage for a
syslogd? <https://rainer.gerhards.net/2008/04/why-is-native-email-capability-an-advantage-for-a-syslogd.html>`_\ "


