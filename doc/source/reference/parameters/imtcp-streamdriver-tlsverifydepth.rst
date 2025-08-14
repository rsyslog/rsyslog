.. _param-imtcp-streamdriver-tlsverifydepth:
.. _imtcp.parameter.module.streamdriver-tlsverifydepth:
.. _imtcp.parameter.input.streamdriver-tlsverifydepth:

StreamDriver.TlsVerifyDepth
===========================

.. index::
   single: imtcp; StreamDriver.TlsVerifyDepth
   single: StreamDriver.TlsVerifyDepth

.. summary-start

Specifies the maximum depth for certificate chain verification.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: StreamDriver.TlsVerifyDepth
:Scope: module, input
:Type: integer
:Default: ``TLS library default`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: 8.2001.0 (module), 8.2106.0 (input)

Description
-----------
Specifies the allowed maximum depth for the certificate chain verification. This feature is supported by the GTLS and OpenSSL drivers. If not set, the underlying TLS library's default will be used.

- For OpenSSL, the default is 100. See the `OpenSSL documentation <https://docs.openssl.org/1.1.1/man3/SSL_CTX_set_verify/>`_ for more details.
- For GnuTLS, the default is 5. See the `GnuTLS documentation <https://www.gnutls.org/manual/gnutls.html>`_ for more details.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

.. note::

   The GnuTLS driver sends all certificates contained in the file
   specified via ``streamDriver.certFile`` (or
   ``$DefaultNetstreamDriverCertFile``) to connecting clients.  To
   expose intermediate certificates, the file must contain the server
   certificate first, followed by the intermediate certificates.
   This capability was added in rsyslog version 8.36.0.

Module usage
------------
.. _imtcp.parameter.module.streamdriver-tlsverifydepth-usage:

.. code-block:: rsyslog

   module(load="imtcp"
          streamDriver.name="gtls"
          streamDriver.mode="1"
          streamDriver.tlsVerifyDepth="5")

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-tlsverifydepth-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.tlsVerifyDepth="3")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
