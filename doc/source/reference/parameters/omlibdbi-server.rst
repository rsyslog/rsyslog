.. _param-omlibdbi-server:
.. _omlibdbi.parameter.input.server:

Server
======

.. index::
   single: omlibdbi; Server
   single: Server

.. summary-start

Specifies the hostname or address of the database server to connect to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omlibdbi`.

:Name: Server
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: Not documented

Description
-----------
Use this to point omlibdbi at the host that runs the selected database
engine. Combine it with the driver, credentials, and database name to
form the full connection information.

Input usage
-----------
.. _param-omlibdbi-input-server-usage:
.. _omlibdbi.parameter.input.server-usage:

.. code-block:: rsyslog

   action(type="omlibdbi" driver="mysql" server="db.example.net"
          uid="dbwriter" pwd="sup3rSecret" db="syslog")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omlibdbi.parameter.legacy.actionlibdbihost:

- $ActionLibdbiHost — maps to Server (status: legacy)

.. index::
   single: omlibdbi; $ActionLibdbiHost
   single: $ActionLibdbiHost

See also
--------
See also :doc:`../../configuration/modules/omlibdbi`.
