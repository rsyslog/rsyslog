.. _param-ompgsql-conninfo:
.. _ompgsql.parameter.input.conninfo:

Conninfo
========

.. index::
   single: ompgsql; Conninfo
   single: Conninfo

.. summary-start

Defines PostgreSQL connection information as a URI or key-value pairs, taking precedence over server, port, database, and password parameters.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ompgsql`.

:Name: Conninfo
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: 8.32.0

Description
-----------
The URI or set of key-value pairs that describe how to connect to the PostgreSQL server. This takes precedence over ``server``, ``port``, ``db``, and ``pass`` parameters. Required if ``server`` and ``db`` are not specified.

The format corresponds to `standard PostgreSQL connection string format <https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING>`_.

Input usage
-----------
.. _param-ompgsql-input-conninfo:
.. _ompgsql.parameter.input.conninfo-usage:

.. code-block:: rsyslog

   module(load="ompgsql")
   action(type="ompgsql" conninfo="postgresql://rsyslog:test1234@localhost/syslog")

See also
--------
See also :doc:`../../configuration/modules/ompgsql`.
