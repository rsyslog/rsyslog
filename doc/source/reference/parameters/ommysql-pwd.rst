.. _param-ommysql-pwd:
.. _ommysql.parameter.module.pwd:

PWD
===

.. index::
   single: ommysql; PWD
   single: PWD

.. summary-start

Specifies the password for the MariaDB/MySQL user defined in :ref:`param-ommysql-uid`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommysql`.

:Name: PWD
:Scope: module
:Type: word
:Default: module=none
:Required?: yes
:Introduced: at least 7.x, possibly earlier

Description
-----------
This is the password for the user specified in UID.

Module usage
------------
.. _param-ommysql-module-pwd:
.. _ommysql.parameter.module.pwd-usage:

.. code-block:: rsyslog

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" db="syslog_db" uid="user" pwd="pwd")

See also
--------
See also :doc:`../../configuration/modules/ommysql`.
