.. _param-imtcp-permittedpeer:
.. _imtcp.parameter.module.permittedpeer:
.. _imtcp.parameter.input.permittedpeer:

PermittedPeer
=============

.. index::
   single: imtcp; PermittedPeer
   single: PermittedPeer

.. summary-start

Specifies a list of permitted peer IDs that are allowed to connect.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: PermittedPeer
:Scope: module, input
:Type: array
:Default: ``none`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2112.0 (input)

Description
-----------
This parameter sets a list of permitted peer IDs. If configured, only peers whose ID matches an entry in the list are able to connect to the listener. The semantics of the ID string depend on the currently selected ``streamDriver.authMode`` and the :doc:`network stream driver <../../concepts/netstrm_drvr>`.

This parameter may not be set in anonymous authentication modes. It can be set to a single peer ID string or an array of peer IDs (either IP addresses or names, depending on the TLS certificate and authentication mode).

**Single Peer:**
``PermittedPeer="127.0.0.1"``

**Array of Peers:**
``PermittedPeer=["test1.example.net","10.1.2.3","test2.example.net"]``

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.permittedpeer-usage:

.. code-block:: rsyslog

   module(load="imtcp"
          streamDriver.name="gtls"
          streamDriver.mode="1"
          streamDriver.authMode="x509/name"
          permittedPeer=["test1.example.net", "test2.example.net"])

Input usage
-----------
.. _imtcp.parameter.input.permittedpeer-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.authMode="x509/name"
         permittedPeer=["peer.local"])

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverstreamdriverpermittedpeer:
- $InputTCPServerStreamDriverPermittedPeer — maps to PermittedPeer (status: legacy)

.. index::
   single: imtcp; $InputTCPServerStreamDriverPermittedPeer
   single: $InputTCPServerStreamDriverPermittedPeer

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
