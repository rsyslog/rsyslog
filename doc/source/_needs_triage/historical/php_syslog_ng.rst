Using php-syslog-ng with rsyslog
================================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net/>`_ *(2005-08-04)*

Note: it has been reported that this guide is somewhat outdated. Most
importantly, this guide is for the **original** php-syslog-ng and
**cannot** be used for it successor logzilla. Please
use the guide with care. Also, please note that **rsyslog's "native" web frontend
is** `Adiscon LogAnalyzer <http://www.phplogcon.org>`_, which provides best
integration and a lot of extra functionality.

Abstract
--------

**In this paper, I describe how to use**
`php-syslog-ng <http://www.vermeer.org/projects/php-syslog-ng>`_ **with**
`rsyslogd <http://www.rsyslog.com/>`_. Php-syslog-ng is a popular web
interface to syslog data. Its name stem from the fact that it usually
picks up its data from a database created by
`syslog-ng <http://www.balabit.com/products/syslog_ng/>`_ and some
helper scripts. However, there is nothing syslog-ng specific in the
database. With rsyslogd's high customizability, it is easy to write to a
syslog-ng like schema. I will tell you how to do this, enabling you to
use php-syslog-ng as a front-end for rsyslogd - or save the hassle with
syslog-ng database configuration and simply go ahead and use rsyslogd
instead.*

Overall System Setup
--------------------

The setup is pretty straightforward. Basically, php-syslog-ng's
interface to the syslogd is the database. We use the schema that
php-syslog-ng expects and make rsyslogd write to it in its format.
Because of this, php-syslog-ng does not even know there is no syslog-ng
present.

Setting up the system
---------------------

For php-syslog-ng, you can follow its usual setup instructions. Just
skip any steps referring to configure syslog-ng. Make sure you create the
database schema in `MariaDB <http://www.mariadb.org/>`_/
`MySQL <http://www.mysql.com/>`_. As of this writing, the expected schema
can be created via this script:

::

  CREATE DATABASE syslog
  USE syslog
  CREATE TABLE logs(host varchar(32) default NULL,
                    facility varchar(10)
                    default NULL,
                    priority varchar(10) default NULL,
                    level varchar(10) default NULL,
                    tag varchar(10) default NULL,
                    date date default NULL,
                    time time default NULL,
                    program varchar(15) default NULL,
                    msg text,
                    seq int(10) unsigned NOT NULL auto_increment,
                    PRIMARY KEY (seq),
                    KEY host (host),
                    KEY seq (seq),
                    KEY program (program),
                    KEY time (time),
                    KEY date (date),
                    KEY priority (priority),
                    KEY facility (facility
                   ) TYPE=MyISAM;``

Please note that at the time you are reading this paper, the schema
might have changed. Check for any differences. As we customize rsyslogd
to the schema, it is vital to have the correct one. If this paper is
outdated, `let me know <mailto:rgerhards@adiscon.com>`_ so that I can
fix it.

Once this schema is created, we simply instruct rsyslogd to store
received data in it. I wont go into too much detail here. If you are
interested in some more details, you might find my paper "`Writing
syslog messages to MySQL <rsyslog_mysql.html>`_\ " worth reading. For
this article, we simply modify `rsyslog.conf <rsyslog_conf.html>`_\ so
that it writes to the database. That is easy. Just these two lines are
needed:

::

  $template syslog-ng,"insert into logs(host, facility, priority, tag, date, time, msg) values ('%HOSTNAME%', %syslogfacility%, %syslogpriority%, '%syslogtag%', '%timereported:::date-mysql%', '%timereported:::date-mysql%', '%msg%')", SQL
  *.*,           mysql-server,syslog,user,pass;syslog-ng

These are just **two** lines. I have color-coded them so that you see
what belongs together (the colors have no other meaning). The green line
is the actual SQL statement being used to take care of the syslog-ng
schema. Rsyslogd allows you to fully control the statement sent to the
database. This allows you to write to any database format, including
your homegrown one (if you so desire). Please note that there is a small
inefficiency in our current usage: the
``'%timereported:::date-mysql%'``
property is used for both the time
and the date (if you wonder about what all these funny characters mean,
see the `rsyslogd property replacer manual <property_replacer.html>`_) .
We could have extracted just the date and time parts of the respective
properties. However, this is more complicated and also adds processing
time to rsyslogd's processing (substrings must be extracted). So we take
a full MariaDB/MySQL-formatted timestamp and supply it to MariaDB/MySQL.
The sql engine in turn discards the unneeded part. It works pretty well.
As of my understanding, the inefficiency of discarding the unneeded part
in MariaDB/MySQL is lower than the efficiency gain from using the full
timestamp in rsyslogd. So it is most probably the best solution.

Please note that rsyslogd knows two different timestamp properties: one
is timereported, used here. It is the timestamp from the message itself.
Sometimes that is a good choice, in other cases not. It depends on your
environment. The other one is the timegenerated property. This is the
time when rsyslogd received the message. For obvious reasons, that
timestamp is consistent, even when your devices are in multiple time
zones or their clocks are off. However, it is not "the real thing". It's
your choice which one you prefer. If you prefer timegenerated ... simply
use it ;)

The line in red tells rsyslogd which messages to log and where to store
it. The "\*.\*" selects all messages. You can use standard syslog
selector line filters here if you do not like to see everything in your
database. The ">" tells rsyslogd that a MariaDB/MySQL connection must be
established. Then, "mysql-server" is the name or IP address of the
server machine, "syslog" is the database name (default from the schema)
and "user" and "pass" are the logon credentials. Use a user with low
privileges, insert into the logs table is sufficient. "syslog-ng" is the
template name and tells rsyslogd to use the SQL statement shown above.

Once you have made the changes, all you need to do is restart rsyslogd.
Then, you should see syslog messages flow into your database - and show
up in php-syslog-ng.

Conclusion
----------

With minimal effort, you can use php-syslog-ng together with rsyslogd.
For those unfamiliar with syslog-ng, this configuration is probably
easier to set up then switching to syslog-ng. For existing rsyslogd
users, php-syslog-ng might be a nice add-on to their logging
infrastructure.

Please note that the `MonitorWare
family <http://www.monitorware.com/en/>`_ (to which rsyslog belongs)
also offers a web-interface: `Adiscon LogAnalyzer`_.
From my point of view, obviously, **phpLogCon is the more natural choice
for a web interface to be used together with rsyslog**. It also offers
superb functionality and provides, for example,native display of Windows
event log entries. I have set up a `demo
server <http://demo.phplogcon.org/>`_., You can have a peek at it
without installing anything.

References and Additional Material
----------------------------------

-  `php-syslog-ng <http://www.vermeer.org/projects/php-syslog-ng>`_
