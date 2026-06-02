.. meta::
   :description: Protect the imhttp health check endpoint with API key authentication.
   :keywords: rsyslog, imhttp, healthcheck, api key authentication

.. _param-imhttp-healthcheckapikeyfile:
.. _imhttp.parameter.module.healthcheckapikeyfile:
.. _imhttp-healthcheckapikeyfile:

healthCheckApiKeyFile
=====================

.. index::
   single: imhttp; healthCheckApiKeyFile
   single: healthCheckApiKeyFile

.. summary-start

Protects the imhttp health check endpoint with API key authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imhttp`.

:Name: healthCheckApiKeyFile
:Scope: module
:Type: string
:Default: none
:Required?: no
:Introduced: Not specified

Description
-----------
If set, the health check endpoint accepts matching API key tokens from a file.
Accepted request headers are ``Authorization: ApiKey`` and ``X-API-Key``.
Leading whitespace after the header scheme is ignored.

Blank lines and lines starting with ``#`` are ignored.

Module usage
------------
.. _imhttp.parameter.module.healthcheckapikeyfile-usage:

.. code-block:: rsyslog

   module(
       load="imhttp"
       healthCheckApiKeyFile="/etc/rsyslog/api-keys.txt"
   )

See also
--------
See also :doc:`../../configuration/modules/imhttp`.
