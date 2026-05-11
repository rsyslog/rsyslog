.. _param-imbeats-streamdriver-tlsverifydepth:
.. _imbeats.parameter.input.streamdriver-tlsverifydepth:

StreamDriver.TlsVerifyDepth
===========================

.. meta::
   :description: Certificate chain depth for imbeats TLS validation.
   :keywords: rsyslog, imbeats, tls verify depth

.. index::
   single: imbeats; StreamDriver.TlsVerifyDepth
   single: StreamDriver.TlsVerifyDepth

.. summary-start

Set the maximum certificate chain depth accepted during imbeats TLS validation.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.TlsVerifyDepth
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the maximum certificate chain depth accepted during imbeats TLS validation.

Input usage
-----------
.. _param-imbeats-input-streamdriver-tlsverifydepth:
.. _imbeats.parameter.input.streamdriver-tlsverifydepth-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.tlsVerifyDepth="5")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
