.. _param-imrelp-tls-authmode:
.. _imrelp.parameter.input.tls-authmode:

tls.authMode
============

.. index::
   single: imrelp; tls.authMode
   single: tls.authMode

.. summary-start

Selects the mutual authentication strategy for TLS-secured RELP sessions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.authMode
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
Sets the mode used for mutual authentication.

Supported values are ``fingerprint``, ``name``, or ``certvalid``.

``fingerprint`` mode basically is what SSH does. It does not require a full PKI
to be present, instead self-signed certs can be used on all peers. Even if a CA
certificate is given, the validity of the peer cert is NOT verified against it.
Only the certificate fingerprint counts.

In ``name`` mode, certificate validation happens. Here, the matching is done
against the certificate's subjectAltName and, as a fallback, the subject common
name. If the certificate contains multiple names, a match on any one of these
names is considered good and permits the peer to talk to rsyslog.

``certvalid`` mode validates the remote peer's certificate chain but does not
check the subject name, so any certificate trusted by the configured CAs is
accepted. This mode is therefore weaker than ``name`` and typically used only
when that reduced verification is acceptable.

Input usage
-----------
.. _param-imrelp-input-tls-authmode-usage:
.. _imrelp.parameter.input.tls-authmode-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.authMode="fingerprint")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
