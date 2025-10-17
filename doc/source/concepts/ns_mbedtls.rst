*****************************
mbedtls Network Stream Driver
*****************************

===========================  ===========================================================================
**Driver Name:**             **mbedtls**
**Author:**                  Stéphane Adenot <stephane.adenot@csgroup.eu>
**Available since:**         8.2512
===========================  ===========================================================================


Purpose
=======

This network stream driver implements a TLS protected transport
via the `MbedTLS library <https://www.trustedfirmware.org/projects/mbed-tls/>`_.


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
   A valid configuration would be e.g.
   ::

      StreamDriverPermittedPeers="SHA256:10:C4:26:1D:CB:3C:AB:12:DB:1A:F0:47:37:AE:6D:D2:DE:66:B5:71:B7:2E:5B:BB:AE:0C:7E:7F:5F:0D:E9:64,SHA1:DD:23:E3:E7:70:F5:B4:13:44:16:78:A5:5A:8C:39:48:53:A6:DD:25"

-  **x509/certvalid** - certificate validation only. x509/certvalid is
   a nonstandard mode. It validates the remote peers certificate, but
   does not check the subject name. This is weak authentication that may
   be useful in scenarios where multiple devices are deployed and it is
   sufficient proof of authenticity when their certificates are signed by
   the CA the server trusts. This is better than anon authentication, but
   still not recommended.

-  **x509/name** - certificate validation and subject name authentication as
   described in IETF's draft-ietf-syslog-transport-tls-12 Internet draft

.. note::

   "anon" does not permit to authenticate the remote peer. As such,
   this mode is vulnerable to man in the middle attacks as well as
   unauthorized access. It is recommended NOT to use this mode.
   A certificate / key does not need to be configured in this auth mode.


CheckExtendedKeyPurpose
=======================

-  **off** - by default this binary argument is turned off, which means
   that Extended Key Usage extension of certificates is ignored
   in cert validation.

-  **on** - if you turn this option on, it will check that peer's certificate
   contains the value for "TLS WWW Client" or "TLS WWW Server"
   respectively, depending whether we are on sending or receiving end of a
   connection.

PrioritizeSAN
=============

-  **off** - by default this binary argument is turned off, which means
   that validation of names in certificates goes per older RFC 5280 and either
   Subject Alternative Name or Common Name match is good and connection is
   allowed.

-  **on** - if you turn this option on, it will perform stricter name checking
   as per newer RFC 6125, where, if any SAN is found, contents of CN are
   completely ignored and name validity is decided based on SAN only.
