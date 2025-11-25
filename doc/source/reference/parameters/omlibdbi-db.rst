.. _param-omlibdbi-db:
.. _omlibdbi.parameter.input.db:

DB
==

.. index::
   single: omlibdbi; DB
   single: DB

.. summary-start

Names the database schema that omlibdbi writes to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omlibdbi`.

:Name: DB
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: Not documented

Description
-----------
Set this to the database instance that should receive the syslog events.
It must exist on the selected server and accept the credentials supplied
via ``UID`` and ``PWD``.

Input usage
-----------
.. _param-omlibdbi-input-db-usage:
.. _omlibdbi.parameter.input.db-usage:

.. code-block:: rsyslog

   action(type="omlibdbi" driver="mysql" server="db.example.net" \
          uid="dbwriter" pwd="sup3rSecret" db="syslog")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omlibdbi.parameter.legacy.actionlibdbidbname:

- $ActionLibdbiDBName — maps to DB (status: legacy)

.. index::
   single: omlibdbi; $ActionLibdbiDBName
   single: $ActionLibdbiDBName

See also
--------
See also :doc:`../../configuration/modules/omlibdbi`.
