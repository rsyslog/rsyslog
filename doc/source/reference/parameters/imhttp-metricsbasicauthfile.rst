.. meta::
   :description: Protect the imhttp metrics endpoint with HTTP Basic Authentication.
   :keywords: rsyslog, imhttp, metrics, basicauthfile, http basic authentication

.. _param-imhttp-metricsbasicauthfile:
.. _imhttp.parameter.module.metricsbasicauthfile:
.. _imhttp-metricsbasicauthfile:

metricsBasicAuthFile
====================

.. index::
   single: imhttp; metricsBasicAuthFile
   single: metricsBasicAuthFile

.. summary-start

Protects the imhttp Prometheus metrics endpoint with HTTP Basic Authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imhttp`.

:Name: metricsBasicAuthFile
:Scope: module
:Type: string
:Default: none
:Required?: no
:Introduced: Not specified

Description
-----------
If set, the metrics endpoint requires HTTP Basic Authentication and expects
credentials from an htpasswd file.

Module usage
------------
.. _imhttp.parameter.module.metricsbasicauthfile-usage:

.. code-block:: rsyslog

   module(
       load="imhttp"
       metricsBasicAuthFile="/etc/rsyslog/htpasswd"
   )

See also
--------
See also :doc:`../../configuration/modules/imhttp`.
