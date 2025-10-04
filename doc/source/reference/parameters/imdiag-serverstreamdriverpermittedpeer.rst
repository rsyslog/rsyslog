.. _param-imdiag-serverstreamdriverpermittedpeer:
.. _imdiag.parameter.input.serverstreamdriverpermittedpeer:

ServerStreamDriverPermittedPeer
===============================

.. index::
   single: imdiag; ServerStreamDriverPermittedPeer
   single: ServerStreamDriverPermittedPeer

.. summary-start

Accepts permitted peer identifiers for compatibility, but the plain TCP driver used by imdiag does not enforce them.

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
authentication. imdiag always selects the plain TCP (``ptcp``) stream driver,
which offers no peer verification. As a result the configured identities are
accepted but ignored. The parameter is kept for forward compatibility with the
generic TCP listener framework should imdiag gain authenticated stream support
in the future.

Configure this parameter before :ref:`ServerRun <param-imdiag-serverrun>` if you
need to preserve configuration compatibility. When multiple peers are listed,
provide an array of identity strings even though the ``ptcp`` driver ignores
them.

Input usage
-----------
.. _param-imdiag-input-serverstreamdriverpermittedpeer:
.. _imdiag.parameter.input.serverstreamdriverpermittedpeer-usage:

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port"
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
