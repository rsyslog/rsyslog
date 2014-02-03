`back <rsyslog_conf_modules.html>`_

MySQL Database Output Module
============================

**Module Name:    ommysql**

**Author:**\ Michael Meckelein (Initial Author) / Rainer Gerhards
<rgerhards@adiscon.com>

**Description**:

This module provides native support for logging to MySQL databases. It
offers superior performance over the more generic
`omlibdbi <omlibdbi.html>`_ module.

**Configuration Directives**:

ommysql mostly uses the "old style" configuration, with almost
everything on the action line itself. A few newer features are being
migrated to the new style-config directive configuration system.

-  **$ActionOmmysqlServerPort <port>**
   Permits to select a non-standard port for the MySQL server. The
   default is 0, which means the system default port is used. There is
   no need to specify this directive unless you know the server is
   running on a non-standard listen port.
-  **$OmMySQLConfigFile <file name>**
   Permits the selection of an optional MySQL Client Library
   configuration file (my.cnf) for extended configuration functionality.
   The use of this configuration directive is necessary only if you have
   a non-standard environment or if fine-grained control over the
   database connection is desired.
-  **$OmMySQLConfigSection <string>**
   Permits the selection of the section within the configuration file
   specified by the **$OmMySQLConfigFile** directive.
   This will likely only be used where the database administrator
   provides a single configuration file with multiple profiles.
   This configuration directive is ignored unless **$OmMySQLConfigFile**
   is also used in the rsyslog configration file.
   If omitted, the MySQL Client Library default of "client" will be
   used.
-  Action parameters:
   **:ommysql:database-server,database-name,database-userid,database-password**
   All parameters should be filled in for a successful connect.

Note rsyslog contains a canned default template to write to the MySQL
database. It works on the MonitorWare schema. This template is:

$template tpl,"insert into SystemEvents (Message, Facility, FromHost,
Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values
('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%,
'%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%,
'%syslogtag%')",SQL

As you can see, the template is an actual SQL statement. Note the ",SQL"
option: it tells the template processor that the template is used for
SQL processing, thus quote characters are quoted to prevent security
issues. You can not assign a template without ",SQL" to a MySQL output
action.

If you would like to change fields contents or add or delete your own
fields, you can simply do so by modifying the schema (if required) and
creating your own custom template.

**Sample:**

The following sample writes all syslog messages to the database
"syslog\_db" on mysqlsever.example.com. The server is being accessed
under the account of "user" with password "pwd".

$ModLoad ommysql $ActionOmmysqlServerPort 1234 # use non-standard port
\*.\*      :ommysql:mysqlserver.example.com,syslog\_db,user,pwd

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008, 2009 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
