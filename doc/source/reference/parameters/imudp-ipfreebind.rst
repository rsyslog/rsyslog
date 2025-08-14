.. _param-imudp-ipfreebind:
.. _imudp.parameter.module.ipfreebind:

IpFreeBind
==========

.. index::
   single: imudp; IpFreeBind
   single: IpFreeBind

.. summary-start

Controls the ``IP_FREEBIND`` socket option when binding to nonlocal addresses.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: IpFreeBind
:Scope: input
:Type: integer
:Default: input=2
:Required?: no
:Introduced: 8.18.0

Description
-----------
Manages the ``IP_FREEBIND`` option on the UDP socket, allowing binding to an IP
address that is nonlocal or not yet configured on any interface.

The parameter accepts these values:

- ``0`` – do not enable ``IP_FREEBIND``; binding fails if the address is
  not available.
- ``1`` – silently enable ``IP_FREEBIND`` only when needed to bind.
- ``2`` – enable ``IP_FREEBIND`` and emit a warning when it was required.

Input usage
-----------
.. _param-imudp-input-ipfreebind:
.. _imudp.parameter.input.ipfreebind:
.. code-block:: rsyslog

   input(type="imudp" IpFreeBind="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.

