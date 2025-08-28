.. _param-imudp-device:
.. _imudp.parameter.input.device:

Device
======

.. index::
   single: imudp; Device
   single: Device

.. summary-start

Binds UDP socket to a specific network device or VRF.

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
Bind socket to given device (e.g., ``eth0``).

For Linux with VRF support, the ``Device`` option can be used to specify the VRF
for the ``Address``.

Examples:

.. code-block:: rsyslog

   module(load="imudp") # needs to be done just once
   input(type="imudp" port="514" device="eth0")

Input usage
-----------
.. _param-imudp-input-device:
.. _imudp.parameter.input.device-usage:

.. code-block:: rsyslog

   input(type="imudp" Device="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
