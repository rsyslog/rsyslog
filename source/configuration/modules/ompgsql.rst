.. index:: ! ompgsql

PostgreSQL Database Output Module (ompgsql)
===========================================

================  ==========================================================================
**Module Name:**  ompgsql
**Author:**       `Rainer Gerhards <rgerhards@adiscon.com>`__ and `Dan Molik <dan@danmolik.com>`__
**Available:**    8.32+
================  ==========================================================================

**Description**:

This module provides native support for logging to PostgreSQL databases. It's an alternative (with potentially superior performance) to the more generic
`omlibdbi <omlibdbi.html>`_ module.


Input parameters
****************

-  **server** - default: none, required: true

   The hostname or address of the PostgreSQL server.

-  **port, serverport** - default: 5432 (PostgreSQL default)

   The IP port of the PostgreSQL server.

-  **db** - default: none, required: true

   The multi-tenant database name to `INSERT` rows into.

-  **user, uid** - default: postgres

   The username to connect to the PostgreSQL server with.

-  **pass, pwd** - default: postgres

   The password to connect to the PostgreSQL server with.

-  **template** - default none, required: false

   The template name to use to `INSERT` rows into the database with. Valid SQL
   syntax is required, as the module does not perform any insertion statement
   checking.


Examples
********

A Basic Example using the internal PostgreSQL template::

  # load module
  module(load="ompgsql")

  action(type="ompgsql" server="localhost"
         user="rsyslog" pass="test1234"
         db="syslog")

A Templated example::

  template(name="sql-syslog" type="list" option.sql="on") {
    constant(value="INSERT INTO SystemEvents (message, timereported) values ('")
    property(name="msg")
    constant(value="','")
    property(name="timereported" dateformat="pgsql" date.inUTC="on")
    constant(value="')")
  }

  # load module
  module(load="ompgsql")

  action(type="ompgsql" server="localhost"
         user="rsyslog" pass="test1234"
         db="syslog"
         template="sql-syslog" )

An action queue and templated example::

  template(name="sql-syslog" type="list" option.sql="on") {
    constant(value="INSERT INTO SystemEvents (message, timereported) values ('")
    property(name="msg")
    constant(value="','")
    property(name="timereported" dateformat="pgsql" date.inUTC="on")
    constant(value="')")
  }

  # load module
  module(load="ompgsql")

  action(type="ompgsql" server="localhost"
         user="rsyslog" pass="test1234"
         db="syslog"
         template="sql-syslog" 
         queue.size="10000" queue.type="linkedList"
         queue.workerthreads="5"
         queue.workerthreadMinimumMessages="500"
         queue.timeoutWorkerthreadShutdown="1000"
         queue.timeoutEnqueue="10000")


Building
********

To compile Rsyslog with PostgreSQL support you will need to:

* install *libpq* and *libpq-dev* packages, check your package manager for the correct name.
* set *--enable-pgsql* switch on configure.


Legacy
******

**Action parameters**

**:ompgsql:database-server,database-name,database-userid,database-password**

All parameters should be filled in for a successful connect.

Note rsyslog contains a canned default template to write to the Postgres
database. This template is:

::

  $template StdPgSQLFmt,"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-pgsql%', '%timegenerated:::date-pgsql%', %iut%, '%syslogtag%')",STDSQL

As you can see, the template is an actual SQL statement. Note the **STDSQL**
option: it tells the template processor that the template is used for
SQL processing, thus quote characters are quoted to prevent security
issues. You can not assign a template without **STDSQL** to a PostgreSQL output
action.

If you would like to change fields contents or add or delete your own
fields, you can simply do so by modifying the schema (if required) and
creating your own custom template:

::

  $template mytemplate,"insert into SystemEvents (Message) values ('%msg%')",STDSQL
  :ompgsql:database-server,database-name,database-userid,database-password;mytemplate

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2008-2014 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.

