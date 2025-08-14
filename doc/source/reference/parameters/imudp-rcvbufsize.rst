.. _param-imudp-rcvbufsize:
.. _imudp.parameter.module.rcvbufsize:

RcvBufSize
==========

.. index::
   single: imudp; RcvBufSize
   single: RcvBufSize

.. summary-start

Requests a specific socket receive buffer size from the operating system.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: RcvBufSize
:Scope: input
:Type: size
:Default: input=0
:Required?: no
:Introduced: 7.3.9

Description
-----------
Asks the OS to allocate a receive buffer of the given size for the UDP socket.
Setting this disables Linux auto-tuning and should be done only when necessary.
Sizes can be specified as plain numbers or with units such as ``256k`` or ``1m``.
Very large values may require root privileges, and the OS may adjust or shrink
them to system limits. Internally, ``setsockopt()`` with ``SO_RCVBUF`` (and
``SO_RCVBUFFORCE`` if available) is used. Maximum value is 1G.

Input usage
-----------
.. _param-imudp-input-rcvbufsize:
.. _imudp.parameter.input.rcvbufsize:
.. code-block:: rsyslog

   input(type="imudp" RcvBufSize="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.

