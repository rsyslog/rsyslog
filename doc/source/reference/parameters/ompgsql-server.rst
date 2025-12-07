.. _param-ompgsql-server:
.. _ompgsql.parameter.module.server:

Server
======

.. index::
   single: ompgsql; Server
   single: Server

.. summary-start

Specifies the hostname or address of the PostgreSQL server when not using a connection string.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ompgsql`.

:Name: Server
:Scope: module
:Type: word
:Default: none
:Required?: no
:Introduced: 8.32.0

Description
-----------
The hostname or address of the PostgreSQL server. Required if ``conninfo`` is not specified.

Module usage
------------
.. _param-ompgsql-module-server:
.. _ompgsql.parameter.module.server-usage:

.. code-block:: rsyslog

   module(load="ompgsql")
   action(type="ompgsql" server="localhost" db="syslog")

See also
--------
See also :doc:`../../configuration/modules/ompgsql`.
