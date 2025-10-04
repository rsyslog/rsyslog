.. _param-imdtls-tls-myprivkey:
.. _imdtls.parameter.input.tls-myprivkey:

tls.myPrivKey
=============

.. index::
   single: imdtls; tls.myPrivKey
   single: tls.myPrivKey

.. summary-start


Points to the private key file paired with ``tls.myCert``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: tls.myPrivKey
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
The private key file corresponding to ``tls.myCert``. This key is used for the
cryptographic operations in the DTLS handshake.

Input usage
-----------
.. _param-imdtls-input-tls-myprivkey:
.. _imdtls.parameter.input.tls-myprivkey-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls"
         tls.myCert="/etc/rsyslog/server.pem"
         tls.myPrivKey="/etc/rsyslog/server.key")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
