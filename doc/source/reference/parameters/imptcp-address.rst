.. _param-imptcp-address:
.. _imptcp.parameter.input.address:

Address
=======

.. index::
   single: imptcp; Address
   single: Address

.. summary-start

Specifies the local address to bind the listener to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Address
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
On multi-homed machines, specifies to which local address the
listener should be bound.

Input usage
-----------
.. _param-imptcp-input-address:
.. _imptcp.parameter.input.address-usage:

.. code-block:: rsyslog

   input(type="imptcp" address="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverlistenip:

- $InputPTCPServerListenIP — maps to Address (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerListenIP
   single: $InputPTCPServerListenIP

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
