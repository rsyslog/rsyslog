.. _param-imudp-device:
.. _imudp.parameter.module.device:

Device
======

.. index::
   single: imudp; Device
   single: Device

.. summary-start

Binds the UDP socket to a specific network device or VRF.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Device
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Specifies the network interface name for the listener; useful for hosts with multiple interfaces or VRF configurations.

Input usage
-----------
.. _param-imudp-input-device:
.. _imudp.parameter.input.device:
.. code-block:: rsyslog

   input(type="imudp" Device="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
