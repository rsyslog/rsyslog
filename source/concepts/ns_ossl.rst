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

-  **0** - unencrypted trasmission (just like `ptcp <ns_ptcp.html>`_ driver)
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
   Even in x509/fingerprint mode, both the client and server certificate
   currently must be signed by the same root CA.

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


