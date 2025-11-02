.. _param-omhttp-tls-cacert:
.. _omhttp.parameter.input.tls-cacert:

tls.cacert
==========

.. index::
   single: omhttp; tls.cacert
   single: tls.cacert

.. summary-start

Points omhttp to the CA bundle used to verify HTTPS servers.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: tls.cacert
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter sets the path to the Certificate Authority (CA) bundle. Expects ``.pem`` format.

Input usage
-----------
.. _omhttp.parameter.input.tls-cacert-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       useHttps="on"
       tlsCaCert="/etc/ssl/certs/ca-bundle.pem"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
