.. _param-imudp-address:
.. _imudp.parameter.input.address:

Address
=======

.. index::
   single: imudp; Address
   single: Address

.. summary-start

Local address that UDP server binds to; ``*`` uses all interfaces.

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
Local IP address (or name) the UDP server should bind to. Use ``*`` to bind to
all of the machine's addresses.

Input usage
-----------
.. _param-imudp-input-address:
.. _imudp.parameter.input.address-usage:

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
