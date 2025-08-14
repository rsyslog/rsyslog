.. _param-imtcp-address:
.. _imtcp.parameter.input.address:

Address
=======

.. index::
   single: imtcp; Address
   single: Address

.. summary-start

Binds the TCP listener to a specific local address on a multi-homed machine.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Address
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
On multi-homed machines, this parameter specifies which local IP address the listener should be bound to. If not set, the listener will accept connections on all available IP addresses.

Input usage
-----------
.. _imtcp.parameter.input.address-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" address="192.168.1.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
