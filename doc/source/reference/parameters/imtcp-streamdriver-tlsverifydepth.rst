.. _param-imtcp-streamdriver-tlsverifydepth:
.. _imtcp.parameter.module.streamdriver-tlsverifydepth:
.. _imtcp.parameter.input.streamdriver-tlsverifydepth:

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
:Scope: module, input
:Type: integer
:Default: module=TLS library default, input=module parameter
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

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-tlsverifydepth:
.. _imtcp.parameter.module.streamdriver-tlsverifydepth-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.tlsVerifyDepth="7")

Input usage
-----------
.. _param-imtcp-input-streamdriver-tlsverifydepth:
.. _imtcp.parameter.input.streamdriver-tlsverifydepth-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" streamDriver.tlsVerifyDepth="7")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

