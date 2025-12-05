.. _param-ommysql-template:
.. _ommysql.parameter.input.template:

Template
========

.. index::
   single: ommysql; Template
   single: Template

.. summary-start

Specifies the template used to format log records for insertion into the MariaDB/MySQL database.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: template
:Scope: input
:Type: word
:Default: input=StdDBFmt
:Required?: no
:Introduced: at least 7.x, possibly earlier

Description
-----------
Rsyslog contains a canned default template to write to the MariaDB/MySQL
database. It works on the MonitorWare schema. This template is:

.. code-block:: rsyslog

   template(name="tpl" type="string"
            string="insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')"
            option.sql="on")

As you can see, the template is an actual SQL statement. Note the ``option.sql="on"``
parameter: it tells the template processor that the template is used for
SQL processing, thus quote characters are quoted to prevent security
issues. You can not assign a template without ``option.sql="on"`` to a MariaDB/MySQL
output action.

If you would like to change fields contents or add or delete your own
fields, you can simply do so by modifying the schema (if required) and
creating your own custom template.

Input usage
-----------
.. _param-ommysql-input-template:
.. _ommysql.parameter.input.template-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   template(name="tpl" type="string"
            string="insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')"
            option.sql="on")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd" template="tpl")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
