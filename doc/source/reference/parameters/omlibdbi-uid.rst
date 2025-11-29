.. _param-omlibdbi-uid:
.. _omlibdbi.parameter.input.uid:

UID
===

.. index::
   single: omlibdbi; UID
   single: UID

.. summary-start

Defines the user name that omlibdbi uses when authenticating to the
database.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omlibdbi`.

:Name: UID
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: Not documented

Description
-----------
Provide the account that has permission to write into the target
database. Combine this with ``PWD`` to supply the password.

Input usage
-----------
.. _param-omlibdbi-input-uid-usage:
.. _omlibdbi.parameter.input.uid-usage:

.. code-block:: rsyslog

   action(type="omlibdbi" driver="mysql" server="db.example.net"
          uid="dbwriter" pwd="sup3rSecret" db="syslog")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omlibdbi.parameter.legacy.actionlibdbiusername:

- $ActionLibdbiUserName — maps to UID (status: legacy)

.. index::
   single: omlibdbi; $ActionLibdbiUserName
   single: $ActionLibdbiUserName

See also
--------
See also :doc:`../../configuration/modules/omlibdbi`.
