.. _param-imdtls-tls-myprivkey:
.. _imdtls.parameter.input.tls-myprivkey:

tls.myprivkey
=============

.. index::
   single: imdtls; tls.myprivkey
   single: tls.myprivkey

.. summary-start


Points to the private key file paired with ``tls.mycert``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: tls.myprivkey
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
The private key file corresponding to ``tls.mycert``. This key is used for the cryptographic operations in the DTLS handshake.

Input usage
-----------
.. _param-imdtls-input-tls-myprivkey:
.. _imdtls.parameter.input.tls-myprivkey-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls"
         tls.mycert="/etc/rsyslog/server.pem"
         tls.myprivkey="/etc/rsyslog/server.key")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
