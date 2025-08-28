.. _param-imudp-rcvbufsize:
.. _imudp.parameter.input.rcvbufsize:

RcvBufSize
==========

.. index::
   single: imudp; RcvBufSize
   single: RcvBufSize

.. summary-start

Requests specific socket receive buffer size; disables auto-tuning.

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
This requests a socket receive buffer of specific size from the operating
system. It is an expert parameter, which should only be changed for a good
reason. Note that setting this parameter disables Linux auto-tuning, which
usually works pretty well. The default value is 0, which means "keep the OS
buffer size unchanged". This is a size value. So in addition to pure integer
values, sizes like ``256k``, ``1m`` and the like can be specified. Note that
setting very large sizes may require root or other special privileges. Also note
that the OS may slightly adjust the value or shrink it to a system-set max value
if the user is not sufficiently privileged. Technically, this parameter will
result in a ``setsockopt()`` call with ``SO_RCVBUF`` (and ``SO_RCVBUFFORCE`` if it
is available). (Maximum Value: 1G)

Examples:

.. code-block:: rsyslog

   module(load="imudp") # needs to be done just once
   input(type="imudp" port="514" rcvbufSize="1m")

Input usage
-----------
.. _param-imudp-input-rcvbufsize:
.. _imudp.parameter.input.rcvbufsize-usage:

.. code-block:: rsyslog

   input(type="imudp" RcvBufSize="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
