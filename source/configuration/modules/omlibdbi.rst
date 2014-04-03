`back <rsyslog_conf_modules.html>`_

Generic Database Output Module (omlibdbi)
=========================================

**Module Name:    omlibdbi**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This modules supports a large number of database systems via
`libdbi <http://libdbi.sourceforge.net/>`_. Libdbi abstracts the
database layer and provides drivers for many systems. Drivers are
available via the
`libdbi-drivers <http://libdbi-drivers.sourceforge.net/>`_ project. As
of this writing, the following drivers are available:

-  `Firebird/Interbase <http://www.firebird.sourceforge.net/>`_
-  `FreeTDS <http://www.freetds.org/>`_ (provides access to `MS SQL
   Server <http://www.microsoft.com/sql>`_ and
   `Sybase <http://www.sybase.com/products/informationmanagement/adaptiveserverenterprise>`_)
-  `MySQL <http://www.mysql.com/>`_ (also supported via the native
   ommysql plugin in rsyslog)
-  `PostgreSQL <http://www.postgresql.org/>`_\ (also supported via the
   native ommysql plugin in rsyslog)
-  `SQLite/SQLite3 <http://www.sqlite.org/>`_

The following drivers are in various stages of completion:

-  `Ingres <http://ingres.com/>`_
-  `mSQL <http://www.hughes.com.au/>`_
-  `Oracle <http://www.oracle.com/>`_

These drivers seem to be quite usable, at least from an rsyslog point of
view.

Libdbi provides a slim layer between rsyslog and the actual database
engine. We have not yet done any performance testing (e.g. omlibdbi vs.
ommysql) but honestly believe that the performance impact should be
irrelevant, if at all measurable. Part of that assumption is that
rsyslog just does the "insert" and most of the time is spent either in
the database engine or rsyslog itself. It's hard to think of any
considerable time spent in the libdbi abstraction layer.

Setup

In order for this plugin to work, you need to have libdbi, the libdbi
driver for your database backend and the client software for your
database backend installed. There are libdbi packages for many
distributions. Please note that rsyslogd requires a quite recent version
(0.8.3) of libdbi. It may work with older versions, but these need some
special ./configure options to support being called from a dlopen()ed
plugin (as omlibdbi is). So in short, you probably save you a lot of
headache if you make sure you have at least libdbi version 0.8.3 on your
system.

**Configuration Directives**:

-  $ActionLibdbiDriverDirectory /path/to/dbd/drivers
   This is a global setting. It points libdbi to its driver directory.
   Usually, you do not need to set it. If you installed libdbi-driver's
   at a non-standard location, you may need to specify the directory
   here. If you are unsure, do not use this configuration directive.
   Usually, everything works just fine.\ ****
-  **$ActionLibdbiDriver drivername**
    Name of the dbidriver to use, see libdbi-drivers documentation. As a
   quick excerpt, at least those were available at the time of this
   writiting "mysql" (suggest to use ommysql instead), "firebird"
   (Firbird and InterBase), "ingres", "msql", "Oracle", "sqlite",
   "sqlite3", "freetds" (for Microsoft SQL and Sybase) and "pgsql"
   (suggest to use ompgsql instead).
-  $ActionLibdbiHost hostname
    The host to connect to.
-  $ActionLibdbiUserName user
    The user used to connect to the database.
-  $ActionlibdbiPassword
    That user's password.
-  $ActionlibdbiDBName db
    The database that shall be written to.
-  selector line: :omlibdbi:;template
    executes the recently configured omlibdbi action. The ;template part
   is optional. If no template is provided, a default template is used
   (which is currently optimized for MySQL - sorry, folks...)

**Caveats/Known Bugs:**

You must make sure that any templates used for omlibdbi properly escape
strings. This is usually done by supplying the SQL (or STDSQL) option to
the template. Omlibdbi rejects templates without this option for
security reasons. However, omlibdbi does not detect if you used the
right option for your backend. Future versions of rsyslog (with
full expression  support) will provide advanced ways of handling this
situation. So far, you must be careful. The default template provided by
rsyslog is suitable for MySQL, but not necessarily for your database
backend. Be careful!

If you receive the rsyslog error message "libdbi or libdbi drivers not
present on this system" you may either not have libdbi and its drivers
installed or (very probably) the version is earlier than 0.8.3. In this
case, you need to make sure you have at least 0.8.3 and the libdbi
driver for your database backend present on your system.

I do not have most of the database supported by omlibdbi in my lab. So
it received limited cross-platform tests. If you run into troubles, be
sure the let us know at
`http://www.rsyslog.com <http://www.rsyslog.com>`_.

**Sample:**

The following sample writes all syslog messages to the database
"syslog\_db" on mysqlsever.example.com. The server is MySQL and being
accessed under the account of "user" with password "pwd" (if you have
empty passwords, just remove the $ActionLibdbiPassword line).

$ModLoad omlibdbi $ActionLibdbiDriver mysql $ActionLibdbiHost
mysqlserver.example.com $ActionLibdbiUserName user $ActionLibdbiPassword
pwd $ActionLibdbiDBName syslog\_db \*.\* :omlibdbi:

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
