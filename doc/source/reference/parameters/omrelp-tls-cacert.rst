.. _param-omrelp-tls-cacert:
.. _omrelp.parameter.input.tls-cacert:

TLS.CaCert
==========

.. index::
   single: omrelp; TLS.CaCert
   single: TLS.CaCert

.. summary-start

Specifies the CA certificate used to verify peer certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.CaCert
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The CA certificate that can verify the machine certs.

.. versionadded:: 8.2008.0

   With librelp 1.7.0, you can use chained certificates. If using "openssl" as tls.tlslib, we recommend at least OpenSSL Version 1.1 or higher. Chained certificates will also work with OpenSSL Version 1.0.2, but they will be loaded into the main OpenSSL context object making them available to all librelp instances (omrelp/imrelp) within the same process.

   If this is not desired, you will require to run rsyslog in multiple instances with different omrelp configurations and certificates.

Input usage
-----------
.. _param-omrelp-input-tls-cacert:
.. _omrelp.parameter.input.tls-cacert-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.caCert="/path/to/ca.pem")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
