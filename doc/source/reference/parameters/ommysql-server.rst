.. _param-ommysql-server:
.. _ommysql.parameter.input.server:

Server
======

.. index::
   single: ommysql; Server
   single: Server

.. summary-start

Sets the MariaDB/MySQL server address the action connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: server
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the hostname or IP address of the MariaDB/MySQL server.

Input usage
-----------
.. _param-ommysql-input-server:
.. _ommysql.parameter.input.server-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
