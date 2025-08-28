.. _param-imudp-ipfreebind:
.. _imudp.parameter.input.ipfreebind:

IpFreeBind
==========

.. index::
   single: imudp; IpFreeBind
   single: IpFreeBind

.. summary-start

Controls Linux ``IP_FREEBIND`` socket option for nonlocal binds.

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
Manages the ``IP_FREEBIND`` option on the UDP socket, which allows binding it to
an IP address that is nonlocal or not (yet) associated to any network interface.

The parameter accepts the following values:

- 0 - does not enable the ``IP_FREEBIND`` option on the UDP socket. If the
  ``bind()`` call fails because of ``EADDRNOTAVAIL`` error, socket initialization
  fails.
- 1 - silently enables the ``IP_FREEBIND`` socket option if it is required to
  successfully bind the socket to a nonlocal address.
- 2 - enables the ``IP_FREEBIND`` socket option and warns when it is used to
  successfully bind the socket to a nonlocal address.

Input usage
-----------
.. _param-imudp-input-ipfreebind:
.. _imudp.parameter.input.ipfreebind-usage:

.. code-block:: rsyslog

   input(type="imudp" IpFreeBind="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
