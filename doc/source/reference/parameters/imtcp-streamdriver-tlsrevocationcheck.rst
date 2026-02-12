.. _param-imtcp-streamdriver-tlsrevocationcheck:
.. _imtcp.parameter.module.streamdriver-tlsrevocationcheck:
.. _imtcp.parameter.input.streamdriver-tlsrevocationcheck:

StreamDriver.TlsRevocationCheck
================================

.. index::
   single: imtcp; StreamDriver.TlsRevocationCheck
   single: StreamDriver.TlsRevocationCheck

.. summary-start

Controls whether TLS certificate revocation checking via OCSP is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.TlsRevocationCheck
:Scope: module, input
:Type: binary
:Default: module=off, input=module parameter
:Required?: no
:Introduced: 8.2602.0

.. warning::

   **EXPERIMENTAL FEATURE**

   This feature is experimental and new in 8.2602.0. It is intended to be
   production-ready, but it has limited real-world practice and may expose
   operational edge cases. Review the details below and test in your
   environment before enabling it broadly.

   * **Bounded I/O**: OCSP uses non-blocking connect with socket timeouts for
     send/receive, but each responder can still add latency (up to
     ``OCSP_TIMEOUT`` seconds).

   * **DoS Attack Vector**: Malicious certificates can contain multiple OCSP
     responder URLs pointing to slow or unresponsive servers, causing
     cumulative latency and potential denial of service.

   * **Thread Blocking**: Timeouts limit the duration, but under load, slow
     responders can still reduce throughput.

   * **Cache behavior**: OCSP responses are cached with expiry based on
     ``nextUpdate`` when available (or a default TTL). This reduces repeated
     network I/O but still depends on network availability for cache misses.

Description
-----------
Controls whether TLS certificate revocation checking is performed during
the TLS handshake. When enabled, rsyslog will query OCSP (Online Certificate
Status Protocol) responders to verify that certificates have not been revoked.

The revocation check is only performed for OpenSSL-based TLS connections
(``StreamDriver.Name="ossl"``). The feature is not available when using
GnuTLS or WolfSSL drivers.

**Important considerations:**

* **Disabled by default**: Certificate revocation checking is disabled by
  default for backward compatibility and to avoid potential performance
  impacts.

* **Bounded I/O**: OCSP checks use non-blocking connect and socket timeouts
  during the TLS handshake. Each responder can still add latency (up to
  ``OCSP_TIMEOUT`` seconds) which may impact throughput and cause connection
  delays.

* **Network requirements**: OCSP checks require outbound network connectivity
  to OCSP responder servers specified in the certificate's Authority
  Information Access extension.

* **Certificate requirements**: Certificates must contain OCSP responder URLs.
  Certificates with only CRL (Certificate Revocation List) distribution points
  are not supported and will fail revocation checks.

* **Caching**: OCSP responses are cached with an expiry based on the response
  ``nextUpdate`` value when available, or a default TTL. This reduces repeat
  lookups but does not eliminate network dependency for cache misses.

When enabled (``"on"``), the TLS handshake will fail if:

- The certificate is revoked
- The OCSP responder is unreachable
- The OCSP response is invalid
- The certificate contains no OCSP responder information but has CRL information

The same-named input parameter can override the module setting.

**Security note**: OCSP checking provides additional security by verifying
that certificates have not been revoked, but introduces operational complexity
and potential availability risks due to external dependencies.

Module usage
------------
.. _param-imtcp-module-streamdriver-tlsrevocationcheck:
.. _imtcp.parameter.module.streamdriver-tlsrevocationcheck-usage:

.. code-block:: rsyslog

   module(load="imtcp" 
          streamDriver.name="ossl"
          streamDriver.mode="1"
          streamDriver.authMode="x509/certvalid"
          streamDriver.tlsRevocationCheck="on")

Input usage
-----------
.. _param-imtcp-input-streamdriver-tlsrevocationcheck:
.. _imtcp.parameter.input.streamdriver-tlsrevocationcheck-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514"
         streamDriver.name="ossl"
         streamDriver.mode="1"
         streamDriver.authMode="x509/certvalid"
         streamDriver.tlsRevocationCheck="on")

Example: Disabled (default)
----------------------------

.. code-block:: rsyslog

   # Explicitly disable revocation checking (same as default)
   module(load="imtcp"
          streamDriver.name="ossl"
          streamDriver.mode="1"
          streamDriver.authMode="x509/certvalid"
          streamDriver.tlsRevocationCheck="off")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
