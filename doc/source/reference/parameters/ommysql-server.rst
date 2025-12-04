.. _param-ommysql-server:
.. _ommysql.parameter.module.server:

Server
======

.. index::
   single: ommysql; Server
   single: Server

.. summary-start

Sets the MariaDB/MySQL server address the action connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: Server
:Scope: module
:Type: word
:Default: module=none
:Required?: yes
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the address of the MariaDB/MySQL-Server.

Module usage
------------
.. _param-ommysql-module-server:
.. _ommysql.parameter.module.server-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
