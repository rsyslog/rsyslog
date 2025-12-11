.. _param-ompgsql-template:
.. _ompgsql.parameter.input.template:

Template
========

.. index::
   single: ompgsql; Template
   single: Template

.. summary-start

Selects the template used for the SQL ``INSERT`` statement sent to PostgreSQL.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ompgsql`.

:Name: Template
:Scope: input
:Type: word
:Default: StdPgSQLFmt
:Required?: no
:Introduced: 8.32.0

Description
-----------
The template name to use to ``INSERT`` rows into the database with. Valid SQL syntax is required, as the module does not perform any insertion statement checking.

Input usage
-----------
.. _ompgsql.parameter.input.template-usage:

.. code-block:: rsyslog

   module(load="ompgsql")
   action(type="ompgsql" server="localhost" db="syslog" template="sql-syslog")

See also
--------
See also :doc:`../../configuration/modules/ompgsql`.
