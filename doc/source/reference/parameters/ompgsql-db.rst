.. _param-ompgsql-db:
.. _ompgsql.parameter.input.db:

db
==

.. index::
   single: ompgsql; db
   single: db

.. summary-start

Names the PostgreSQL database into which rows are inserted when not specified in a connection string.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ompgsql`.

:Name: db
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: 8.32.0

Description
-----------
The multi-tenant database name to ``INSERT`` rows into. Required if ``conninfo`` is not specified.

Input usage
-----------
.. _ompgsql.parameter.input.db-usage:

.. code-block:: rsyslog

   module(load="ompgsql")
   action(type="ompgsql" server="localhost" db="syslog")

See also
--------
See also :doc:`../../configuration/modules/ompgsql`.
