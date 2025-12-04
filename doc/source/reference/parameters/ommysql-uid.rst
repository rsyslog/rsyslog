.. _param-ommysql-uid:
.. _ommysql.parameter.module.uid:

UID
===

.. index::
   single: ommysql; UID
   single: UID

.. summary-start

Defines the MariaDB/MySQL user account used for the connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: UID
:Scope: module
:Type: word
:Default: module=none
:Required?: yes
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the user who is used.

Module usage
------------
.. _param-ommysql-module-uid:
.. _ommysql.parameter.module.uid-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
