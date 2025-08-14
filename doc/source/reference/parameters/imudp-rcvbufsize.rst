.. _param-imudp-rcvbufsize:
.. _imudp.parameter.module.rcvbufsize:

RcvBufSize
==========

.. index::
   single: imudp; RcvBufSize
   single: RcvBufSize

.. summary-start

Requests a specific socket receive buffer size for incoming UDP packets.

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
Overrides the operating system's automatic buffer sizing by requesting a fixed size; excessively large values may require elevated privileges.

Input usage
-----------
.. _param-imudp-input-rcvbufsize:
.. _imudp.parameter.input.rcvbufsize:
.. code-block:: rsyslog

   input(type="imudp" RcvBufSize="...")

Notes
-----
.. versionadded:: 7.3.9

See also
--------
See also :doc:`../../configuration/modules/imudp`.
