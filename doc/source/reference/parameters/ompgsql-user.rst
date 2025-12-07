.. _param-ompgsql-user:
.. _ompgsql.parameter.input.user:

User
====

.. index::
   single: ompgsql; User
   single: User

.. summary-start

Sets the username used to authenticate to the PostgreSQL server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ompgsql`.

:Name: User
:Scope: input
:Type: word
:Default: postgres
:Required?: no
:Introduced: 8.32.0

Description
-----------
The username to connect to the PostgreSQL server with. The name ``UID`` is also accepted.

Input usage
-----------
.. _param-ompgsql-input-user:
.. _ompgsql.parameter.input.user-usage:

.. code-block:: rsyslog

   module(load="ompgsql")
   action(type="ompgsql" server="localhost" user="rsyslog" db="syslog")

See also
--------
See also :doc:`../../configuration/modules/ompgsql`.
