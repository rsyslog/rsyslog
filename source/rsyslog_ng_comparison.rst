`back <features.html>`_

rsyslog vs. syslog-ng
=====================

*Written by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
(2008-05-06), slightly updated 2012-01-09*

**This comparison page is rooted nearly 5 years in the past and has
become severely outdated since then.** It was unmaintained for several
years and contained false information on both syslog-ng and rsyslog as
technology had advanced so much.

This page was initially written because so many people asked about a
comparison when rsyslog was in its infancy. So I tried to create one,
but it was hard to maintain as both projects grew and added feature
after feature. I have to admit we did not try hard to keep it current --
there were many other priorities. I even had forgetten about this page,
when I saw that Peter Czanik blogged about its
`incorrectness <http://blogs.balabit.com/2012/01/05/rsyslog-vs-syslog-ng/>`_
(it must be noted that Peter is wrong on RELP -- it is well alive). I
now remember that he asked me some time ago about this page, what I
somehow lost... I guess he must have been rather grumpy about that :-(

Visiting this page after so many years is interesting, because it shows
how much has changed since then. Obviously, one of my main goals in
regard to syslog-ng is reached: in 2007, I blogged that `the world needs
another
syslogd <http://blog.gerhards.net/2007/08/why-does-world-need-another-syslogd.html>`_
in order to have healthy competition and a greate feature set in the
free editions. In my opinion, the timeline clearly tells that rsyslog's
competition has driven more syslog-ng features from the commercial to
the free edition. Also, I found it interesting to see that syslog-ng has
adapted rsyslog's licensing scheme, modular design and
multi-threadedness. On the other hand, the Balabit folks have obviously
done a quicker and better move on log normalization with what they call
patterndb (it is very roughly equivalent to what rsyslog has just
recently introduced with the help of liblognorm).

To that account, I think the projects are closer together than 5 years
ago. I should now go ahead and create a new feature comparison. Given
previous experience, I think this does not work out. In the future, we
will probably focus on some top features, as Balabit does. However, that
requires some time and I have to admit I do not like to drop this page
that has a lot of inbound links. So I think I do the useful thing by
providing these notes and removing the syslog-ng information. So it
can't be wrong on syslog-ng any more. Note that it still contains some
incorrect information about rsyslog (it's the state it had 5 years
ago!). The core idea is to start with updating the `rsyslog feature
sheet <features.html>`_ and from there on work to a complete
comparision. Of course, feel free to read on if you like to get some
sense of history (and inspiration on what you can still do -- but more
;)).
 Thanks,
 Rainer Gerhards

**Feature**

**rsyslog**

**syslog-ng**

 **Input Sources**

UNIX domain socket

yes

UDP

yes

TCP

yes

`RELP <http://www.librelp.com>`_

yes

RFC 3195/BEEP

yes (via `im3195 <im3195.html>`_)

kernel log

yes

file

yes

mark message generator as an optional input

yes

Windows Event Log

via a Windows event logging software such as
`EventReporter <http://www.eventreporter.com>`_ or `MonitorWare
Agent <http://www.mwagent.com>`_ (both commercial software, both fund
rsyslog development)

**
 Network (Protocol) Support**

support for (plain) tcp based syslog

yes

support for GSS-API

yes

ability to limit the allowed network senders (syslog ACLs)

yes

support for syslog-transport-tls based framing on syslog/tcp connections

yes

udp syslog

yes

syslog over RELP
 truly reliable message delivery (`Why is plain tcp syslog not
reliable? <http://blog.gerhards.net/2008/05/why-you-cant-build-reliable-tcp.html>`_)

yes

on the wire (zlib) message compression

yes

support for receiving messages via reliable `RFC
3195 <http://www.monitorware.com/Common/en/glossary/rfc3195.php>`_
delivery

yes

support for `TLS/SSL-protected syslog <rsyslog_tls.html>`_

`natively <rsyslog_tls.html>`_ (since 3.19.0)
`via stunnel <rsyslog_stunnel.html>`_

support for IETF's new syslog-protocol draft

yes

support for IETF's new syslog-transport-tls draft

yes
(since 3.19.0 - world's first implementation)

support for IPv6

yes

native ability to send SNMP traps

yes

ability to preserve the original hostname in NAT environments and relay
chains

yes

 **Message Filtering**

Filtering for syslog facility and priority

yes

Filtering for hostname

yes

Filtering for application

yes

Filtering for message contents

yes

Filtering for sending IP address

yes

ability to filter on any other message field not mentioned above
(including substrings and the like)

yes

support for complex filters, using full boolean algebra with and/or/not
operators and parenthesis

yes

Support for reusable filters: specify a filter once and use it in
multiple selector lines

no

support for arbritrary complex arithmetic and string expressions inside
filters

yes

ability to use regular expressions in filters

yes

support for discarding messages based on filters

yes

ability to filter out messages based on sequence of appearing

yes (starting with 3.21.3)

powerful BSD-style hostname and program name blocks for easy multi-host
support

yes

 **Supported Database Outputs**

MySQL

`yes <rsyslog_mysql.html>`_ (native
ommysql, \ `omlibdbi <omlibdbi.html>`_)

PostgreSQL

yes (native ompgsql, \ `omlibdbi <omlibdbi.html>`_)

Oracle

yes (`omlibdbi <omlibdbi.html>`_)

SQLite

yes (`omlibdbi <omlibdbi.html>`_)

Microsoft SQL (Open TDS)

yes (`omlibdbi <omlibdbi.html>`_)

Sybase (Open TDS)

yes (`omlibdbi <omlibdbi.html>`_)

Firebird/Interbase

yes (`omlibdbi <omlibdbi.html>`_)

Ingres

yes (`omlibdbi <omlibdbi.html>`_)

mSQL

yes (`omlibdbi <omlibdbi.html>`_)

 **Enterprise Features**

support for on-demand on-disk spooling of messages

yes

ability to limit disk space used by spool files

yes

each action can use its own, independant set of spool files

yes

different sets of spool files can be placed on different disk

yes

ability to process spooled messages only during a configured timeframe
(e.g. process messages only during off-peak hours, during peak hours
they are enqueued only)

`yes <http://wiki.rsyslog.com/index.php/OffPeakHours>`_
 (can independently be configured for the main queue and each action
queue)

ability to configure backup syslog/database servers

yes

Professional Support

`yes <professional_support.html>`_

 **Config File**

config file format

compatible to legacy syslogd but ugly

ability to include config file from within other config files

yes

ability to include all config files existing in a specific directory

yes

 **Extensibility**

Functionality split in separately loadable modules

yes

Support for third-party input plugins

yes

Support for third-party output plugins

yes

 **Other Features**

ability to generate file names and directories (log targets) dynamically

yes

control of log output format, including ability to present channel and
priority as visible log data

yes

native ability to send mail messages

yes (`ommail <ommail.html>`_, introduced in 3.17.0)

good timestamp format control; at a minimum, ISO 8601/RFC 3339
second-resolution UTC zone

yes

ability to reformat message contents and work with substrings

yes

support for log files larger than 2gb

yes

support for log file size limitation and automatic rollover command
execution

yes

support for running multiple syslogd instances on a single machine

yes

ability to execute shell scripts on received messages

yes

ability to pipe messages to a continously running program

massively multi-threaded for tomorrow's multi-core machines

yes

ability to control repeated line reduction ("last message repeated n
times") on a per selector-line basis

yes

supports multiple actions per selector/filter condition

yes

web interface

`phpLogCon <http://www.phplogcon.org>`_
 [also works with
`php-syslog-ng <http://freshmeat.net/projects/php-syslog-ng/>`_]

using text files as input source

yes

rate-limiting output actions

yes

discard low-priority messages under system stress

yes

flow control (slow down message reception when system is busy)

yes (advanced, with multiple ways to slow down inputs depending on
individual input capabilities, based on watermarks)

rewriting messages

yes

output data into various formats

yes

ability to control "message repeated n times" generation

yes

license

GPLv3 (GPLv2 for v2 branch)

supported platforms

Linux, BSD, anecdotical seen on Solaris; compilation and basic testing
done on HP UX

DNS cache

While the rsyslog project was initiated in 2004, it is build on the main
author's (Rainer Gerhards) 12+ years of logging experience. Rainer, for
example, also wrote the first `Windows syslog
server <http://www.winsyslog.com/Common/en/News/WinSyslog-1996-03-31.php>`_
in early 1996 and invented the
`eventlog-to-syslog <http://www.eventreporter.com/Common/en/News/EvntSLog-1997-03-23.php>`_
class of applications in early 1997. He did custom logging development
and consulting even before he wrote these products. Rsyslog draws on
that vast experience and sometimes even on the code.

Based on a discussion I had, I also wrote about the **political argument
why it is good to have another strong syslogd besides syslog-ng**. You
may want to read it at my blog at "`Why does the world need another
syslogd? <http://rgerhards.blogspot.com/2007/08/why-does-world-need-another-syslogd.html>`_\ ".

[`manual index <manual.html>`_\ ]
[`rsyslog.conf <rsyslog_conf.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
