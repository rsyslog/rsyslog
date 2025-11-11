.. _param-omdtls-tls-cacert:
.. _omdtls.parameter.input.tls-cacert:

tls.cacert
==========

.. index::
   single: omdtls; tls.cacert
   single: tls.cacert

.. summary-start

Defines the CA certificate used to verify client certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: tls.cacert
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
The CA certificate that is being used to verify the client certificates.
This parameter is required when :ref:`tls.authmode <param-omdtls-tls-authmode>`
is set to ``fingerprint``, ``name``, or ``certvalid``.

Input usage
-----------
.. _omdtls.parameter.input.tls-cacert-usage:

.. code-block:: rsyslog

   action(type="omdtls"
          target="192.0.2.1"
          tls.authMode="certvalid"
          tls.caCert="/etc/rsyslog/ca.pem")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
