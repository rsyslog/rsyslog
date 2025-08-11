*********************************************
ommysql: MariaDB/MySQL Database Output Module
*********************************************

===========================  ===========================================================================
**Module Name:**             **ommysql**
**Author:**                  Michael Meckelein (Initial Author) / `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for logging to MariaDB/MySQL 
databases. It offers superior performance over the more generic
`omlibdbi <omlibdbi.html>`_ module.


Configuration Parameters
========================

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

   "word", "none", "yes", "none"

This is the address of the MariaDB/MySQL-Server.


Socket
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the unix socket path of the MariaDB/MySQL-Server. When the server
address is set localhost, the MariaDB/MySQL client library connects using
the default unix socket specified at build time.
If you run MariaDB/MySQL server and run the unix socket path differently
than the default, you can set the socket path with this option.


db
^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

This is the name of the database used.


UID
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

This is the user who is used.


PWD
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

This is the password for the user specified in UID.


ServerPort
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "none", "no", "``$ActionOmmysqlServerPort``"

Permits to select a non-standard port for the MariaDB/MySQL server. The
default is 0, which means the system default port is used. There is
no need to specify this parameter unless you know the server is
running on a non-standard listen port.


MySQLConfig.File
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$OmMySQLConfigFile``"

Permits the selection of an optional MariaDB/MySQL Client Library
configuration file (my.cnf) for extended configuration functionality.
The use of this configuration parameter is necessary only if you have
a non-standard environment or if fine-grained control over the
database connection is desired.


MySQLConfig.Section
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$OmMySQLConfigSection``"

Permits the selection of the section within the configuration file
specified by the **$OmMySQLConfigFile** parameter.
This will likely only be used where the database administrator
provides a single configuration file with multiple profiles.
This configuration parameter is ignored unless **$OmMySQLConfigFile**
is also used in the rsyslog configuration file.
If omitted, the MariaDB/MySQL Client Library default of "client" will be
used.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "StdDBFmt", "no", "none"

Rsyslog contains a canned default template to write to the MariaDB/MySQL
database. It works on the MonitorWare schema. This template is:

.. code-block:: none

   $template tpl,"insert into SystemEvents (Message, Facility, FromHost,
   Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values
   ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%,
   '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%,
   '%syslogtag%')",SQL


As you can see, the template is an actual SQL statement. Note the ",SQL"
option: it tells the template processor that the template is used for
SQL processing, thus quote characters are quoted to prevent security
issues. You can not assign a template without ",SQL" to a MariaDB/MySQL 
output action.

If you would like to change fields contents or add or delete your own
fields, you can simply do so by modifying the schema (if required) and
creating your own custom template.


Examples
========

Example 1
---------

The following sample writes all syslog messages to the database
"syslog_db" on mysqlserver.example.com. The server is being accessed
under the account of "user" with password "pwd".

.. code-block:: none

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" serverport="1234"
          db="syslog_db" uid="user" pwd="pwd")



FAQ
===

* :doc:`How can I encrypt the rsyslog connection to MariaDB/MySQL? <../../faq/encrypt_mysql_traffic_ommysql>`
