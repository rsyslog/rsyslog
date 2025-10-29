.. _param-imrelp-tls-compression:
.. _imrelp.parameter.input.tls-compression:

tls.compression
===============

.. index::
   single: imrelp; tls.compression
   single: tls.compression

.. summary-start

Controls whether TLS sessions compress payload data before transmission.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.compression
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not documented

Description
-----------
This controls if the TLS stream should be compressed (zipped). While this
increases CPU use, the network bandwidth should be reduced. Note that typical
text-based log records usually compress rather well.

Input usage
-----------
.. _param-imrelp-input-tls-compression:
.. _imrelp.parameter.input.tls-compression-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.compression="on")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
