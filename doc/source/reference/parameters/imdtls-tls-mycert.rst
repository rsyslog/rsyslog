.. _param-imdtls-tls-mycert:
.. _imdtls.parameter.input.tls-mycert:

tls.mycert
==========

.. index::
   single: imdtls; tls.mycert
   single: tls.mycert

.. summary-start


Identifies the certificate file the imdtls listener presents to peers.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: tls.mycert
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Specifies the certificate file used by imdtls. This certificate is presented to peers during the DTLS handshake.

Input usage
-----------
.. _param-imdtls-input-tls-mycert:
.. _imdtls.parameter.input.tls-mycert-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls"
         tls.mycert="/etc/rsyslog/server.pem"
         tls.myprivkey="/etc/rsyslog/server.key")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
