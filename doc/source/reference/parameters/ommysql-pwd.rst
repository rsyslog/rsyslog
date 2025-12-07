.. _param-ommysql-pwd:
.. _ommysql.parameter.input.pwd:

PWD
===

.. index::
   single: ommysql; PWD
   single: PWD

.. summary-start

Specifies the password for the MariaDB/MySQL user defined in :ref:`param-ommysql-uid`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: pwd
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the password for the user account specified in the :ref:`param-ommysql-uid` parameter, used to authenticate with the MariaDB/MySQL server.

Input usage
-----------
.. _ommysql.parameter.input.pwd-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
