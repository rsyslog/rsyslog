ompgsql: PostgreSQL Database Output Module
==========================================

**Module Name:** ompgsql

**Author:** Rainer Gerhards
<rgerhards@adiscon.com>

**Description**:

This module provides native support for logging to PostgreSQL databases. It's an alternative (with potentially superior performance) to the more generic
`omlibdbi <omlibdbi.html>`_ module.

**Configuration Directives**:

ompgsql uses the "old style" configuration, with everything on the action line itself

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

