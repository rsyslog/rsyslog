.. _param-imtcp-streamdriver-tlsverifydepth:
.. _imtcp.parameter.module.streamdriver-tlsverifydepth:

StreamDriver.TlsVerifyDepth
===========================

.. index::
   single: imtcp; StreamDriver.TlsVerifyDepth
   single: StreamDriver.TlsVerifyDepth

.. summary-start

Specifies the maximum depth allowed for certificate chain verification.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.TlsVerifyDepth
:Scope: module
:Type: integer
:Default: module=TLS library default
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the allowed maximum depth for the certificate chain verification.
Support added in v8.2001.0, supported by GTLS and OpenSSL driver.
If not set, the API default will be used.
For OpenSSL, the default is 100 - see the doc for more:
https://docs.openssl.org/1.1.1/man3/SSL_CTX_set_verify/
For GnuTLS, the default is 5 - see the doc for more:
https://www.gnutls.org/manual/gnutls.html

.. note::

   The GnuTLS driver sends all certificates contained in the file
   specified via ``StreamDriver.CertFile`` (or
   ``$DefaultNetstreamDriverCertFile``) to connecting clients.  To
   expose intermediate certificates, the file must contain the server
   certificate first, followed by the intermediate certificates.
   This capability was added in rsyslog version 8.36.0.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-tlsverifydepth:
.. _imtcp.parameter.module.streamdriver-tlsverifydepth-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.tlsVerifyDepth="7")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

