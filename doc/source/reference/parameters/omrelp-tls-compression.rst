.. _param-omrelp-tls-compression:
.. _omrelp.parameter.input.tls-compression:

TLS.Compression
===============

.. index::
   single: omrelp; TLS.Compression
   single: TLS.Compression

.. summary-start

Controls whether the TLS stream is compressed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.Compression
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The controls if the TLS stream should be compressed (zipped). While this increases CPU use, the network bandwidth should be reduced. Note that typical text-based log records usually compress rather well.

Input usage
-----------
.. _param-omrelp-input-tls-compression:
.. _omrelp.parameter.input.tls-compression-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.compression="on")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
