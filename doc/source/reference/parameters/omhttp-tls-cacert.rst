.. _param-omhttp-tls-cacert:
.. _omhttp.parameter.module.tls-cacert:

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
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter sets the path to the Certificate Authority (CA) bundle. Expects ``.pem`` format.

Module usage
------------
.. _param-omhttp-module-tls-cacert:
.. _omhttp.parameter.module.tls-cacert-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       useHttps="on"
       tls.caCert="/etc/ssl/certs/ca-bundle.pem"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
