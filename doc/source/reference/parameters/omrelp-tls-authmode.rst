.. _param-omrelp-tls-authmode:
.. _omrelp.parameter.input.tls-authmode:

TLS.AuthMode
============

.. index::
   single: omrelp; TLS.AuthMode
   single: TLS.AuthMode

.. summary-start

Chooses the mutual authentication mode (fingerprint or name) for TLS.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.AuthMode
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Sets the mode used for mutual authentication. Supported values are either "*fingerprint*" or "*name*". Fingerprint mode basically is what SSH does. It does not require a full PKI to be present, instead self-signed certs can be used on all peers. Even if a CA certificate is given, the validity of the peer cert is NOT verified against it. Only the certificate fingerprint counts.

In "name" mode, certificate validation happens. Here, the matching is done against the certificate's subjectAltName and, as a fallback, the subject common name. If the certificate contains multiple names, a match on any one of these names is considered good and permits the peer to talk to rsyslog.

The permitted names or fingerprints are configured via `TLS.PermittedPeer`.

Input usage
-----------
.. _param-omrelp-input-tls-authmode:
.. _omrelp.parameter.input.tls-authmode-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.authMode="fingerprint")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
