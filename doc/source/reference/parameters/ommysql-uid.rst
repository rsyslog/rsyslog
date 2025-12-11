.. _param-ommysql-uid:
.. _ommysql.parameter.input.uid:

UID
===

.. index::
   single: ommysql; UID
   single: UID

.. summary-start

Defines the MariaDB/MySQL user account used for the connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: uid
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the username used to authenticate with the MariaDB/MySQL server.

Input usage
-----------
.. _ommysql.parameter.input.uid-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
