`back <features.html>`_

Handling a massive syslog database insert rate with Rsyslog
===========================================================

*Written by* `Rainer Gerhards <http://www.gerhards.net/rainer>`_
*(2008-01-31)*

Abstract
--------

**In this paper, I describe how log massive amounts of**
`syslog <http://www.monitorware.com/en/topics/syslog/>`_ **messages to a
database.**\ This HOWTO is currently under development and thus a bit
brief. Updates are promised ;).*

The Intention
-------------

Database updates are inherently slow when it comes to storing syslog
messages. However, there are a number of applications where it is handy
to have the message inside a database. Rsyslog supports native database
writing via output plugins. As of this writing, there are plugins
available for MySQL an PostgreSQL. Maybe additional plugins have become
available by the time you read this. Be sure to check.

In order to successfully write messages to a database backend, the
backend must be capable to record messages at the expected average
arrival rate. This is the rate if you take all messages that can arrive
within a day and divide it by 86400 (the number of seconds per day).
Let's say you expect 43,200,000 messages per day. That's an average rate
of 500 messages per second (mps). Your database server MUST be able to
handle that amount of message per second on a sustained rate. If it
doesn't, you either need to add an additional server, lower the number
of message - or forget about it.

However, this is probably not your peak rate. Let's simply assume your
systems work only half a day, that's 12 hours (and, yes, I know this is
unrealistic, but you'll get the point soon). So your average rate is
actually 1,000 mps during work hours and 0 mps during non-work hours. To
make matters worse, workload is not divided evenly during the day. So
you may have peaks of up to 10,000mps while at other times the load may
go down to maybe just 100mps. Peaks may stay well above 2,000mps for a
few minutes.

So how the hack you will be able to handle all of this traffic
(including the peaks) with a database server that is just capable of
inserting a maximum of 500mps?

The key here is buffering. Messages that the database server is not
capable to handle will be buffered until it is. Of course, that means
database insert are NOT real-time. If you need real-time inserts, you
need to make sure your database server can handle traffic at the actual
peak rate. But lets assume you are OK with some delay.

Buffering is fine. But how about these massive amounts of data? That
can't be hold in memory, so don't we run out of luck with buffering? The
key here is that rsyslog can not only buffer in memory but also buffer
to disk (this may remind you of "spooling" which gets you the right
idea). There are several queuing modes available, offering differnent
throughput. In general, the idea is to buffer in memory until the memory
buffer is exhausted and switch to disk-buffering when needed (and only
as long as needed). All of this is handled automatically and
transparently by rsyslog.

With our above scenario, the disk buffer would build up during the day
and rsyslog would use the night to drain it. Obviously, this is an
extreme example, but it shows what can be done. Please note that queue
content survies rsyslogd restarts, so even a reboot of the system will
not cause any message loss.

How To Setup
------------

Frankly, it's quite easy. You just need to do is instruct rsyslog to use
a disk queue and then configure your action. There is nothing else to
do. With the following simple config file, you log anything you receive
to a MySQL database and have buffering applied automatically.

$ModLoad ommysql # load the output driver (use ompgsql for PostgreSQL)
$ModLoad imudp # network reception $UDPServerRun 514 # start a udp
server at port 514 $ModLoad imuxsock # local message reception
$WorkDirectory /rsyslog/work # default location for work (spool) files
$MainMsgQueueFileName mainq # set file name, also enables disk mode
$ActionResumeRetryCount -1 # infinite retries on insert failure # for
PostgreSQL replace :ommysql: by :ompgsql: below: \*.\*
:ommysql:hostname,dbname,userid,password;

The simple setup above has one drawback: the write database action is
executed together with all other actions. Typically, local files are
also written. These local file writes are now bound to the speed of the
database action. So if the database is down, or threre is a large
backlog, local files are also not (or late) written.

**There is an easy way to avoid this with rsyslog.** It involves a
slightly more complicated setup. In rsyslog, each action can utilize its
own queue. If so, messages are simply pulled over from the main queue
and then the action queue handles action processing on its own. This
way, main processing and the action are de-coupled. In the above
example, this means that local file writes will happen immediately while
the database writes are queued. As a side-note, each action can have its
own queue, so if you would like to more than a single database or send
messages reliably to another host, you can do all of this on their own
queues, de-coupling their processing speeds.

The configuration for the de-coupled database write involves just a few
more commands:

$ModLoad ommysql # load the output driver (use ompgsql for PostgreSQL)
$ModLoad imudp # network reception $UDPServerRun 514 # start a udp
server at port 514 $ModLoad imuxsock # local message reception
$WorkDirectory /rsyslog/work # default location for work (spool) files
$ActionQueueType LinkedList # use asynchronous processing
$ActionQueueFileName dbq # set file name, also enables disk mode
$ActionResumeRetryCount -1 # infinite retries on insert failure # for
PostgreSQL replace :ommysql: by :ompgsql: below: \*.\*
:ommysql:hostname,dbname,userid,password;

**This is the recommended configuration for this use case.** It requires
rsyslog 3.11.0 or above.

In this example, the main message queue is NOT disk-assisted (there is
no $MainMsgQueueFileName directive). We still could do that, but have
not done it because there seems to be no need. The only slow running
action is the database writer and it has its own queue. So there is no
real reason to use a large main message queue (except, of course, if you
expect \*really\* heavy traffic bursts).

Note that you can modify a lot of queue performance parameters, but the
above config will get you going with default values. If you consider
using this on a real busy server, it is strongly recommended to invest
some time in setting the tuning parameters to appropriate values.

Feedback requested
~~~~~~~~~~~~~~~~~~

I would appreciate feedback on this tutorial. If you have additional
ideas, comments or find bugs (I \*do\* bugs - no way... ;)), please `let
me know <mailto:rgerhards@adiscon.com>`_.

Revision History
----------------

-  2008-01-28 \* `Rainer
   Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ \*
   Initial Version created
-  2008-01-28 \* `Rainer
   Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ \*
   Updated to new v3.11.0 capabilities

Copyright
---------

Copyright (c) 2008 `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ and
`Adiscon <http://www.adiscon.com/en/>`_.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.2 or
any later version published by the Free Software Foundation; with no
Invariant Sections, no Front-Cover Texts, and no Back-Cover Texts. A
copy of the license can be viewed at
`http://www.gnu.org/copyleft/fdl.html <http://www.gnu.org/copyleft/fdl.html>`_.

[`manual index <manual.html>`_\ ]
[`rsyslog.conf <rsyslog_conf.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
