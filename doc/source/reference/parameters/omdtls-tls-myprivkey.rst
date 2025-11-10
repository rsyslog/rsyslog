.. _param-omdtls-tls-myprivkey:
.. _omdtls.parameter.input.tls-myprivkey:

tls.myprivkey
=============

.. index::
   single: omdtls; tls.myprivkey
   single: tls.myprivkey

.. summary-start

Provides the private key that matches ``tls.mycert`` for DTLS authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: tls.myprivkey
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
The private key file corresponding to :ref:`tls.mycert <param-omdtls-tls-mycert>`.
This key is used for the cryptographic operations in the DTLS handshake.

Input usage
-----------
.. _omdtls.parameter.input.tls-myprivkey-usage:

.. code-block:: rsyslog

   action(type="omdtls"
          target="192.0.2.1"
          tls.myCert="/etc/rsyslog/omdtls.crt"
          tls.myPrivKey="/etc/rsyslog/omdtls.key")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
