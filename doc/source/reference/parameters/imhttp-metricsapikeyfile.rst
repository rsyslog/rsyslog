.. meta::
   :description: Protect the imhttp metrics endpoint with API key authentication.
   :keywords: rsyslog, imhttp, metrics, api key authentication

.. _param-imhttp-metricsapikeyfile:
.. _imhttp.parameter.module.metricsapikeyfile:
.. _imhttp-metricsapikeyfile:

metricsApiKeyFile
=================

.. index::
   single: imhttp; metricsApiKeyFile
   single: metricsApiKeyFile

.. summary-start

Protects the imhttp Prometheus metrics endpoint with API key authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imhttp`.

:Name: metricsApiKeyFile
:Scope: module
:Type: string
:Default: none
:Required?: no
:Introduced: Not specified

Description
-----------
If set, the metrics endpoint accepts matching API key tokens from a file.
Accepted request headers are ``Authorization: ApiKey`` and ``X-API-Key``.
Leading whitespace after the header scheme is ignored.

Blank lines and lines starting with ``#`` are ignored.

Module usage
------------
.. _imhttp.parameter.module.metricsapikeyfile-usage:

.. code-block:: rsyslog

   module(
       load="imhttp"
       metricsApiKeyFile="/etc/rsyslog/api-keys.txt"
   )

See also
--------
See also :doc:`../../configuration/modules/imhttp`.
