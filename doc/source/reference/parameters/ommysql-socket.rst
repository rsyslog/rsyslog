.. _param-ommysql-socket:
.. _ommysql.parameter.module.socket:

Socket
======

.. index::
   single: ommysql; Socket
   single: Socket

.. summary-start

Sets the Unix socket path to use when connecting to the MariaDB/MySQL server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: Socket
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the unix socket path of the MariaDB/MySQL server. When the server
address is set localhost, the MariaDB/MySQL client library connects using
the default unix socket specified at build time.
If you run MariaDB/MySQL server and run the unix socket path differently
than the default, you can set the socket path with this option.

Module usage
------------
.. _param-ommysql-module-socket:
.. _ommysql.parameter.module.socket-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="localhost" socket="/var/run/mysqld/mysqld.sock" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
