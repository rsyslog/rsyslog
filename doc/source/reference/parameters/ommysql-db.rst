.. _param-ommysql-db:
.. _ommysql.parameter.module.db:

db
==

.. index::
   single: ommysql; db
   single: db

.. summary-start

Names the MariaDB/MySQL database that receives the log records.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: db
:Scope: module
:Type: word
:Default: module=none
:Required?: yes
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the name of the database on the MariaDB/MySQL server where log records will be inserted.

Module usage
------------
.. _param-ommysql-module-db:
.. _ommysql.parameter.module.db-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
