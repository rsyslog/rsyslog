.. _param-imtcp-permittedpeer:
.. _imtcp.parameter.module.permittedpeer:
.. _imtcp.parameter.input.permittedpeer:

PermittedPeer
=============

.. index::
   single: imtcp; PermittedPeer
   single: PermittedPeer

.. summary-start

Restricts connections to listed peer identities.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: PermittedPeer
:Scope: module, input
:Type: array
:Default: module=none, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets permitted peer IDs. Only these peers are able to connect to
the listener. <id-string> semantics depend on the currently
selected AuthMode and
:doc:`network stream driver <../../concepts/netstrm_drvr>`.
PermittedPeer may not be set in anonymous modes. PermittedPeer may
be set either to a single peer or an array of peers either of type
IP or name, depending on the tls certificate.

Single peer:
PermittedPeer="127.0.0.1"

Array of peers:
PermittedPeer=["test1.example.net","10.1.2.3","test2.example.net","..."]

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-permittedpeer:
.. _imtcp.parameter.module.permittedpeer-usage:

.. code-block:: rsyslog

   module(load="imtcp" permittedPeer="127.0.0.1")

Input usage
-----------
.. _param-imtcp-input-permittedpeer:
.. _imtcp.parameter.input.permittedpeer-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" permittedPeer="127.0.0.1")

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

