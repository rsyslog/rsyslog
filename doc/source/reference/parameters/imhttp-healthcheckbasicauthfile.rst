.. meta::
   :description: Protect the imhttp health check endpoint with HTTP Basic Authentication.
   :keywords: rsyslog, imhttp, healthcheck, basicauthfile, http basic authentication

.. _param-imhttp-healthcheckbasicauthfile:
.. _imhttp.parameter.module.healthcheckbasicauthfile:
.. _imhttp-healthcheckbasicauthfile:

healthCheckBasicAuthFile
========================

.. index::
   single: imhttp; healthCheckBasicAuthFile
   single: healthCheckBasicAuthFile

.. summary-start

Protects the imhttp health check endpoint with HTTP Basic Authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imhttp`.

:Name: healthCheckBasicAuthFile
:Scope: module
:Type: string
:Default: none
:Required?: no
:Introduced: Not specified

Description
-----------
If set, the health check endpoint requires HTTP Basic Authentication and
expects credentials from an htpasswd file.

Module usage
------------
.. _imhttp.parameter.module.healthcheckbasicauthfile-usage:

.. code-block:: rsyslog

   module(
       load="imhttp"
       healthCheckBasicAuthFile="/etc/rsyslog/htpasswd"
   )

See also
--------
See also :doc:`../../configuration/modules/imhttp`.
