`back <rsyslog_conf_modules.html>`_

Linux Journal Output Module (omjournal)
=======================================

**Module Name:    omjournal**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Available since**: 7.3.7

**Description**:

The omjournal output module provides an interface to the Linux journal.
It is meant to be used in those cases where the Linux journal is being
used as the sole system log database. With omjournal, messages from
various sources (e.g. files and remote devices) can also be written to
the journal and processed by its tools.

A typical use case we had on our mind is a SOHO environment, where the
user wants to include syslog data obtained from the local router to be
part of the journal data.

We suggest to check out our short presentation on `rsyslog journal
integration <http://youtu.be/GTS7EuSdFKE>`_ to learn more details of
anticipated use cases.

 

**Module Configuration Parameters**:

Currently none.

 

**Action Confguration Parameters**:

Currently none.

**Caveats/Known Bugs:**

-  One needs to be careful that no message routing loop is created. The
   systemd journal forwards messages it receives to the traditional
   syslog system (if present). That means rsyslog will receive the same
   message that it just wrote as new input on imuxsock. If not handled
   specially and assuming all messages be written to the journal, the
   message would be emitted to the journal again and a deadly loop is
   started.
   To prevent that, imuxsock by default does not accept messages
   originating from its own process ID, aka it ignores messages from the
   current instance of rsyslogd. However, this setting can be changed,
   and if so the problem may occur.

**Sample:**

We assume we have a DSL router inside the network and would like to
receive its syslog message into the journal. Note that this
configuration can be used without havoing any other syslog functionality
at all (most importantly, there is no need to write any file to
/var/log!). We assume syslog over UDP, as this is the most probable
choice for the SOHO environment that this use case reflects. To log to
syslog data to the journal, add the following snippet to rsyslog.conf:
/\* first, we make sure all necessary \* modules are present: \*/
module(load="imudp") # input module for UDP syslog
module(load="omjournal") # output module for journal /\* then, define
the actual server that listens to the \* router. Note that 514 is the
default port for UDP \* syslog and that we use a dedicated ruleset to \*
avoid mixing messages with the local log stream \* (if there is any).
\*/ input(type="imudp" port="514" ruleset="writeToJournal") /\* inside
that ruleset, we just write data to the journal: \*/
ruleset(name="writeToJournal") { action(type="omjournal") }

Note that this can be your sole rsyslog.conf if you do not use rsyslog
for anything else than receving the router syslog messages.

If you do not receive messages, **you probably need to enable inbound
UDP syslog traffic in your firewall**.

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2013 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
