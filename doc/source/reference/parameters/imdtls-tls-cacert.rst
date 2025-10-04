.. _param-imdtls-tls-cacert:
.. _imdtls.parameter.input.tls-cacert:

tls.cacert
==========

.. index::
   single: imdtls; tls.cacert
   single: tls.cacert

.. summary-start


Specifies the CA certificate file used to verify DTLS client certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: tls.cacert
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
The CA certificate that is being used to verify the client certificates. This
file must be configured if ``tls.authMode`` is set to ``fingerprint``, ``name``
or ``certvalid``.

Input usage
-----------
.. _param-imdtls-input-tls-cacert:
.. _imdtls.parameter.input.tls-cacert-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls"
         tls.cacert="/etc/rsyslog/ca.pem"
         tls.authMode="certvalid")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
