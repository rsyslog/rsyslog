.. _param-omlibdbi-driver:
.. _omlibdbi.parameter.input.driver:

Driver
======

.. index::
   single: omlibdbi; Driver
   single: Driver

.. summary-start

Selects the libdbi driver backend to use for this action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omlibdbi`.

:Name: Driver
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: Not documented

Description
-----------
Set this to the driver name that matches your database backend, as
documented by libdbi-drivers. Common values include:

- ``mysql`` (:doc:`../../configuration/modules/ommysql` is recommended
  instead)
- ``firebird`` (Firebird and InterBase)
- ``ingres``
- ``msql``
- ``Oracle``
- ``sqlite``
- ``sqlite3``
- ``freetds`` (Microsoft SQL and Sybase)
- ``pgsql`` (:doc:`../../configuration/modules/ompgsql` is recommended
  instead)

Input usage
-----------
.. _param-omlibdbi-input-driver-usage:
.. _omlibdbi.parameter.input.driver-usage:

.. code-block:: rsyslog

   action(type="omlibdbi" driver="mysql" server="db.example.net" \
          uid="dbwriter" pwd="sup3rSecret" db="syslog")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omlibdbi.parameter.legacy.actionlibdbidriver:

- $ActionLibdbiDriver — maps to Driver (status: legacy)

.. index::
   single: omlibdbi; $ActionLibdbiDriver
   single: $ActionLibdbiDriver

See also
--------
See also :doc:`../../configuration/modules/omlibdbi`.
