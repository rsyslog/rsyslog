.. meta::
   :description: Tutorial for native post-quantum TLS in rsyslog with OpenSSL and GnuTLS on supported distros.
   :keywords: rsyslog, post-quantum, pq, ml-kem, openssl 3.5, gnutls, tls

Native Post-Quantum TLS
=======================

.. summary-start

This tutorial shows how to run native post-quantum TLS with rsyslog on
supported newer distros using the existing TLS configuration surface. It covers
the support policy, OpenSSL 3.5 hybrid groups, GnuTLS hybrid groups, and basic
verification steps.

.. summary-end

Post-quantum support in rsyslog currently follows a native-only policy:

- Use PQ or hybrid TLS only where the distro already ships the required
  OpenSSL or GnuTLS support in regular packages.
- Do not expect rsyslog to load external PQ providers for older distro
  versions.
- If you need PQ support, move to a distro baseline that already ships it.

Supported baseline
------------------

At the time this tutorial was written, the intended native baselines were:

- Fedora 43 or newer for native OpenSSL 3.5 hybrid groups.
- Debian 13 or newer for native OpenSSL 3.5 hybrid groups.
- Supported native GnuTLS builds that expose
  ``GROUP-X25519-MLKEM768`` in priority strings.

Older distro versions are intentionally out of scope for this first phase.
If there is a real need, older-version support can be revisited later.

OpenSSL example
---------------

This example keeps classical X.509 certificates and enables a native OpenSSL
hybrid TLS 1.3 group on supported distros.

Server:

.. code-block:: rsyslog

   global(
       defaultNetstreamDriverCAFile="/etc/rsyslog.d/ca.pem"
       defaultNetstreamDriverCertFile="/etc/rsyslog.d/server-cert.pem"
       defaultNetstreamDriverKeyFile="/etc/rsyslog.d/server-key.pem"
   )

   module(
       load="imtcp"
       gnutlsPriorityString="MinProtocol=TLSv1.3
   MaxProtocol=TLSv1.3
   Groups=X25519MLKEM768"
   )

   input(
       type="imtcp"
       port="6514"
       StreamDriver.Name="ossl"
       StreamDriver.Mode="1"
       StreamDriver.AuthMode="x509/certvalid"
   )

Client:

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="logs.example.net"
       port="6514"
       protocol="tcp"
       StreamDriver="ossl"
       StreamDriverMode="1"
       StreamDriverAuthMode="x509/certvalid"
       StreamDriver.CAFile="/etc/rsyslog.d/ca.pem"
       StreamDriver.CertFile="/etc/rsyslog.d/client-cert.pem"
       StreamDriver.KeyFile="/etc/rsyslog.d/client-key.pem"
       gnutlsPriorityString="MinProtocol=TLSv1.3
   MaxProtocol=TLSv1.3
   Groups=X25519MLKEM768"
   )

GnuTLS example
--------------

This example uses the native GnuTLS hybrid group syntax on supported native
GnuTLS builds.

Server:

.. code-block:: rsyslog

   global(
       defaultNetstreamDriverCAFile="/etc/rsyslog.d/ca.pem"
       defaultNetstreamDriverCertFile="/etc/rsyslog.d/server-cert.pem"
       defaultNetstreamDriverKeyFile="/etc/rsyslog.d/server-key.pem"
       defaultNetstreamDriver="gtls"
   )

   module(
       load="imtcp"
       gnutlsPriorityString="NORMAL:-GROUP-ALL:+GROUP-X25519-MLKEM768:+GROUP-X25519"
   )

   input(
       type="imtcp"
       port="6514"
       StreamDriver.Name="gtls"
       StreamDriver.Mode="1"
       StreamDriver.AuthMode="x509/certvalid"
   )

Client:

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="logs.example.net"
       port="6514"
       protocol="tcp"
       StreamDriver="gtls"
       StreamDriverMode="1"
       StreamDriverAuthMode="x509/certvalid"
       StreamDriver.CAFile="/etc/rsyslog.d/ca.pem"
       StreamDriver.CertFile="/etc/rsyslog.d/client-cert.pem"
       StreamDriver.KeyFile="/etc/rsyslog.d/client-key.pem"
       gnutlsPriorityString="NORMAL:-GROUP-ALL:+GROUP-X25519-MLKEM768:+GROUP-X25519"
   )

How to verify
-------------

1. Check the native library baseline first.

   OpenSSL:

   .. code-block:: bash

      openssl version
      openssl list -tls-groups | grep X25519MLKEM768

   GnuTLS:

   .. code-block:: bash

      gnutls-cli --version
      gnutls-cli --priority 'NORMAL:-GROUP-ALL:+GROUP-X25519-MLKEM768:+GROUP-X25519' --list

2. Start the server and client configuration.
3. Send a test message.
4. If rsyslog logs an error that the priority string option or OpenSSL command
   is unsupported, the native distro library does not provide the requested PQ
   group on that system.

Notes
-----

- This tutorial targets hybrid key exchange first. It does not promise native
  PQ certificates or signatures.
- The same ``gnutlsPriorityString`` parameter is used for both OpenSSL and
  GnuTLS, but the string format is TLS-library specific.
- If you operate older distro versions, stay on classical TLS for now or plan a
  distro upgrade before enabling PQ.
