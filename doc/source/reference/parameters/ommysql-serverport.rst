.. _param-ommysql-serverport:
.. _ommysql.parameter.input.serverport:

ServerPort
==========

.. index::
   single: ommysql; ServerPort
   single: ServerPort

.. summary-start

Selects a non-standard port for the MariaDB/MySQL server connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: serverport
:Scope: input
:Type: integer
:Default: input=0 (system default port)
:Required?: no
:Introduced: at least 7.x, possibly earlier

Description
-----------
Permits to select a non-standard port for the MariaDB/MySQL server. The
default is 0, which means the system default port is used. There is
no need to specify this parameter unless you know the server is
running on a non-standard listen port.

Input usage
-----------
.. _ommysql.parameter.input.serverport-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" serverPort="1234" db="syslog_db" uid="user" pwd="pwd")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommysql.parameter.legacy.actionommysqlserverport:

- $ActionOmmysqlServerPort — maps to ServerPort (status: legacy)

.. index::
   single: ommysql; $ActionOmmysqlServerPort
   single: $ActionOmmysqlServerPort

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
