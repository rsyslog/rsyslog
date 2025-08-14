.. _param-imudp-address:
.. _imudp.parameter.module.address:

Address
=======

.. index::
   single: imudp; Address
   single: Address

.. summary-start

Binds the UDP listener to a specific local address or `*` for all interfaces.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Address
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the IP address or hostname used when binding the socket; use `*` to listen on every available address.

Input usage
-----------
.. _param-imudp-input-address:
.. _imudp.parameter.input.address:
.. code-block:: rsyslog

   input(type="imudp" Address="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imudp.parameter.legacy.udpserveraddress:

- $UDPServerAddress — maps to Address (status: legacy)

.. index::
   single: imudp; $UDPServerAddress
   single: $UDPServerAddress

See also
--------
See also :doc:`../../configuration/modules/imudp`.
