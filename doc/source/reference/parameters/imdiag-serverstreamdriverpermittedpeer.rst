.. _param-imdiag-serverstreamdriverpermittedpeer:
.. _imdiag.parameter.input.serverstreamdriverpermittedpeer:

ServerStreamDriverPermittedPeer
===============================

.. index::
   single: imdiag; ServerStreamDriverPermittedPeer
   single: ServerStreamDriverPermittedPeer

.. summary-start

Restricts the diagnostic listener to the listed peer identities.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ServerStreamDriverPermittedPeer
:Scope: input
:Type: array
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Defines the set of remote peers that may connect when the chosen
:doc:`network stream driver <../../concepts/netstrm_drvr>` supports
authentication (for example, TLS certificate verification). Peer identifiers
must match the expectations of the active authentication mode; their syntax is
stream-driver specific.

Configure this parameter before :ref:`ServerRun <param-imdiag-serverrun>`. When
multiple peers are allowed, provide an array of identity strings.

Input usage
-----------
.. _param-imdiag-input-serverstreamdriverpermittedpeer:
.. _imdiag.parameter.input.serverstreamdriverpermittedpeer-usage:

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port"
         serverStreamDriverAuthMode="x509/name"
         serverStreamDriverPermittedPeer=["diag.example.com","127.0.0.1"]
         serverRun="19998")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiagserverstreamdriverpermittedpeer:

- $IMDiagServerStreamDriverPermittedPeer — maps to ServerStreamDriverPermittedPeer (status: legacy)

.. index::
   single: imdiag; $IMDiagServerStreamDriverPermittedPeer
   single: $IMDiagServerStreamDriverPermittedPeer

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
