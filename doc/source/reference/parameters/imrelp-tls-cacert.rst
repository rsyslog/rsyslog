.. _param-imrelp-tls-cacert:
.. _imrelp.parameter.input.tls-cacert:

TLS.CaCert
==========

.. index::
   single: imrelp; TLS.CaCert
   single: TLS.CaCert

.. summary-start

Specifies the CA certificate file used to validate client certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: TLS.CaCert
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
The CA certificate that is being used to verify the client certificates. Has to
be configured if TLS.AuthMode is set to "*fingerprint*" or "*name*".

Input usage
-----------
.. _param-imrelp-input-tls-cacert:
.. _imrelp.parameter.input.tls-cacert-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.caCert="/etc/rsyslog/ca.pem")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
