.. _param-omdtls-tls-mycert:
.. _omdtls.parameter.input.tls-mycert:

tls.mycert
==========

.. index::
   single: omdtls; tls.mycert
   single: tls.mycert

.. summary-start

Specifies the certificate file presented by omdtls during the DTLS handshake.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: tls.mycert
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Specifies the certificate file used by omdtls. This certificate is presented to
peers during the DTLS handshake.

Input usage
-----------
.. _omdtls.parameter.input.tls-mycert-usage:

.. code-block:: rsyslog

   action(type="omdtls"
          target="192.0.2.1"
          port="4433"
          tls.myCert="/etc/rsyslog/omdtls.crt")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
