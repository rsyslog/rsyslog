*****************************
openssl Network Stream Driver
*****************************

===========================  ===========================================================================
**Driver Name:**             **ossl**
**Author:**                  Andre Lorbach <alorbach@adiscon.com>
**Available since:**         8.36.0
===========================  ===========================================================================


Purpose
=======

This network stream driver implements a TLS protected transport
via the `OpenSSL library <https://www.openssl.org/>`_.


Supported Driver Modes
======================

-  **0** - unencrypted transmission (just like `ptcp <ns_ptcp.html>`_ driver)
-  **1** - TLS-protected operation

.. note::

   Mode 0 does not provide any benefit over the ptcp driver. This
   mode exists for technical reasons, but should not be used. It may be
   removed in the future.


Supported Authentication Modes
==============================

-  **anon** - anonymous authentication as described in IETF's
   draft-ietf-syslog-transport-tls-12 Internet draft

-  **x509/fingerprint** - certificate fingerprint authentication as
   described in IETF's draft-ietf-syslog-transport-tls-12 Internet draft.
   The fingerprint must be provided as the SHA1 or the SHA256 hex string of
   the certificate. Multiple values must be separated by comma (,).
   A valid configuration would be e.G.
   ::

      StreamDriverPermittedPeers="SHA256:10:C4:26:1D:CB:3C:AB:12:DB:1A:F0:47:37:AE:6D:D2:DE:66:B5:71:B7:2E:5B:BB:AE:0C:7E:7F:5F:0D:E9:64,SHA1:DD:23:E3:E7:70:F5:B4:13:44:16:78:A5:5A:8C:39:48:53:A6:DD:25"

-  **x509/certvalid** - certificate validation only. x509/certvalid is
   a nonstandard mode. It validates the remote peers certificate, but
   does not check the subject name. This is weak authentication that may
   be useful in scenarios where multiple devices are deployed and it is
   sufficient proof of authenticity when their certificates are signed by
   the CA the server trusts. This is better than anon authentication, but
   still not recommended. **Known Problems**

-  **x509/name** - certificate validation and subject name authentication as
   described in IETF's draft-ietf-syslog-transport-tls-12 Internet draft

.. note::

   "anon" does not permit to authenticate the remote peer. As such,
   this mode is vulnerable to man in the middle attacks as well as
   unauthorized access. It is recommended NOT to use this mode.
   A certificate / key does not need to be configured in this authmode.

.. note::

   **Anon mode changes in:** v8.190 (or above)

   -  Anonymous Ciphers (DH and ECDH) are available in ANON mode.
   -  Server does not require a certificate anymore in anon mode.
   -  If Server has a certificate and the Client does not, the highest possible
      ciphers will be selected.
   -  If both Server and Client do not have a certificate, the highest available
      anon cipher will be used.


