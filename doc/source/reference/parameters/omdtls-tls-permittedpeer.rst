.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Configure the expected DTLS server name or SHA-256 certificate pin for omdtls.
   :keywords: rsyslog, omdtls, DTLS, tls.permittedpeer, server identity

.. _param-omdtls-tls-permittedpeer:

tls.permittedpeer
=================

.. summary-start

Expected DTLS server identity for ``name`` or SHA-256 pin for ``fingerprint``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: tls.permittedpeer
:Scope: action
:Type: string
:Default: unset
:Required?: required for ``fingerprint``; optional for ``name``

For ``name``, provide one DNS name or IP address. If omitted, ``target`` is
the expected identity. A DNS name is useful when ``target`` is an IP address
or routing alias. For ``fingerprint``, provide a complete SHA-256 certificate
pin as ``SHA256:`` followed by 32 colon-separated hexadecimal octets. Empty,
short or malformed pins are rejected during configuration.

For a server reached by IP but identified by the DNS name in its certificate,
set the expected name explicitly. Supply the CA certificate that signed the
server certificate and the client certificate/key required by your server.

RainerScript example::

   module(load="omdtls")
   action(type="omdtls" target="192.0.2.10" port="4433"
          tls.authmode="name" tls.permittedpeer="logs.example.net"
          tls.cacert="/etc/rsyslog.d/ca.pem"
          tls.mycert="/etc/rsyslog.d/client.pem"
          tls.myprivkey="/etc/rsyslog.d/client.key")

Equivalent YAML example::

   version: 2
   modules:
     - load: omdtls
   rulesets:
     - name: main
       actions:
         - type: omdtls
           target: "192.0.2.10"
           port: "4433"
           tls.authmode: "name"
           tls.permittedpeer: "logs.example.net"
           tls.cacert: "/etc/rsyslog.d/ca.pem"
           tls.mycert: "/etc/rsyslog.d/client.pem"
           tls.myprivkey: "/etc/rsyslog.d/client.key"

For ``fingerprint``, use the same action in either format, change
``tls.authmode`` to ``fingerprint``, and set ``tls.permittedpeer`` to the
actual server certificate's SHA-256 fingerprint, for example
``SHA256:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00``.
The value shown is illustrative; replace it with the fingerprint of the
server certificate you intend to trust.
