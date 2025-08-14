.. _param-imudp-address:
.. _imudp.parameter.module.address:

Address
=======

.. index::
   single: imudp; Address
   single: Address

.. summary-start

Local IP address or name the UDP socket binds to; use ``*`` for all addresses.

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
Specifies the local IP address or host name to which the UDP server binds. A
value of ``*`` binds to all addresses on the machine.

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

