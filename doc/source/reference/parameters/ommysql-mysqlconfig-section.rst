.. _param-ommysql-mysqlconfig-section:
.. _ommysql.parameter.input.mysqlconfig-section:

MySQLConfig.Section
===================

.. index::
   single: ommysql; MySQLConfig.Section
   single: MySQLConfig.Section

.. summary-start

Chooses the section within the client configuration file specified by :ref:`param-ommysql-mysqlconfig-file`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: mysqlconfig.section
:Scope: input
:Type: word
:Default: input=client
:Required?: no
:Introduced: at least 7.x, possibly earlier

Description
-----------
Permits the selection of the section within the configuration file
specified by the :ref:`param-ommysql-mysqlconfig-file` parameter (legacy name
**$OmMySQLConfigFile**).
This will likely only be used where the database administrator
provides a single configuration file with multiple profiles.
This configuration parameter is ignored unless
:ref:`param-ommysql-mysqlconfig-file` (legacy name **$OmMySQLConfigFile**)
is also used in the rsyslog configuration file.
If omitted, the MariaDB/MySQL Client Library default of "client" will be
used.

Input usage
-----------
.. _param-ommysql-input-mysqlconfig-section:
.. _ommysql.parameter.input.mysqlconfig-section-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql"
          server="mysqlserver.example.com"
          mysqlConfig.file="/etc/mysql/my.cnf"
          mysqlConfig.section="custom-client"
          db="syslog_db" uid="user" pwd="pwd")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommysql.parameter.legacy.ommysqlconfigsection:
- $OmMySQLConfigSection — maps to MySQLConfig.Section (status: legacy)

.. index::
   single: ommysql; $OmMySQLConfigSection
   single: $OmMySQLConfigSection

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
