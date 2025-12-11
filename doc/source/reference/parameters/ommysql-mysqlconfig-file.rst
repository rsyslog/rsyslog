.. _param-ommysql-mysqlconfig-file:
.. _ommysql.parameter.input.mysqlconfig-file:

MySQLConfig.File
================

.. index::
   single: ommysql; MySQLConfig.File
   single: MySQLConfig.File

.. summary-start

Selects an optional MariaDB/MySQL client configuration file (my.cnf) for the connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: mysqlconfig.file
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: at least 7.x, possibly earlier

Description
-----------
Permits the selection of an optional MariaDB/MySQL Client Library
configuration file (my.cnf) for extended configuration functionality.
The use of this configuration parameter is necessary only if you have
a non-standard environment or if fine-grained control over the
database connection is desired.

Input usage
-----------
.. _ommysql.parameter.input.mysqlconfig-file-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql"
          server="mysqlserver.example.com"
          mysqlConfig.file="/etc/mysql/my.cnf"
          db="syslog_db" uid="user" pwd="pwd")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommysql.parameter.legacy.ommysqlconfigfile:
- $OmMySQLConfigFile — maps to MySQLConfig.File (status: legacy)

.. index::
   single: ommysql; $OmMySQLConfigFile
   single: $OmMySQLConfigFile

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
