.. _param-ompgsql-port:
.. _ompgsql.parameter.input.port:

Port
====

.. index::
   single: ompgsql; Port
   single: Port

.. summary-start

Sets the TCP port of the PostgreSQL server (also accepted as ``serverport``).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ompgsql`.

:Name: Port
:Scope: input
:Type: integer
:Default: 5432
:Required?: no
:Introduced: 8.32.0

Description
-----------
The IP port of the PostgreSQL server. The name ``serverport`` is also accepted.

Input usage
-----------
.. _param-ompgsql-input-port:
.. _ompgsql.parameter.input.port-usage:

.. code-block:: rsyslog

   module(load="ompgsql")
   action(type="ompgsql" server="localhost" port="5432" db="syslog")

See also
--------
See also :doc:`../../configuration/modules/ompgsql`.
