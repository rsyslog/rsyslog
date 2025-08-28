.. _param-imudp-batchsize:
.. _imudp.parameter.module.batchsize:

BatchSize
=========

.. index::
   single: imudp; BatchSize
   single: BatchSize

.. summary-start

Maximum messages retrieved per ``recvmmsg()`` call when available.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: BatchSize
:Scope: module
:Type: integer
:Default: module=32
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This parameter is only meaningful if the system supports ``recvmmsg()`` (newer
Linux systems do this). The parameter is silently ignored if the system does not
support it. If supported, it sets the maximum number of UDP messages that can be
obtained with a single OS call. For systems with high UDP traffic, a relatively
high batch size can reduce system overhead and improve performance. However,
this parameter should not be overdone. For each buffer, max message size bytes
are statically required. Also, a too-high number leads to reduced efficiency, as
some structures need to be completely initialized before the OS call is done. We
would suggest to not set it above a value of 128, except if experimental results
show that this is useful.

Module usage
------------
.. _param-imudp-module-batchsize:
.. _imudp.parameter.module.batchsize-usage:

.. code-block:: rsyslog

   module(load="imudp" BatchSize="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
