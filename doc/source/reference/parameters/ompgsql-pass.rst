.. _param-ompgsql-pass:
.. _ompgsql.parameter.input.pass:

Pass
====

.. index::
   single: ompgsql; Pass
   single: Pass

.. summary-start

Provides the password used for PostgreSQL authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ompgsql`.

:Name: Pass
:Scope: input
:Type: word
:Default: postgres
:Required?: no
:Introduced: 8.32.0

Description
-----------
The password to connect to the PostgreSQL server with. The name ``PWD`` is also accepted.

Input usage
-----------
.. _ompgsql.parameter.input.pass-usage:

.. code-block:: rsyslog

   module(load="ompgsql")
   action(type="ompgsql" server="localhost" pass="test1234" db="syslog")

See also
--------
See also :doc:`../../configuration/modules/ompgsql`.
