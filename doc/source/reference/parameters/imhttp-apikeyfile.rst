.. meta::
   :description: Configure imhttp to protect an input with API key authentication.
   :keywords: rsyslog, imhttp, apikeyfile, api key authentication, http

.. _param-imhttp-apikeyfile:
.. _imhttp.parameter.input.apikeyfile:
.. _imhttp-apikeyfile:

apiKeyFile
==========

.. index::
   single: imhttp; apiKeyFile
   single: apiKeyFile

.. summary-start

Protects an imhttp input with API key authentication using tokens listed in a file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imhttp`.

:Name: apiKeyFile
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: Not specified

Description
-----------
If set, imhttp reads one API key token per line from the specified file and
accepts matching HTTP ``Authorization: ApiKey`` or ``X-API-Key`` credentials.
Leading whitespace after ``ApiKey`` and ``X-API-Key`` is ignored.

Blank lines and lines starting with ``#`` are ignored.

If both ``basicAuthFile`` and ``apiKeyFile`` are configured for the same input,
either authentication method is accepted.

Input usage
-----------
.. _imhttp.parameter.input.apikeyfile-usage:

.. code-block:: rsyslog

   input(
       type="imhttp"
       endpoint="/ingest"
       apiKeyFile="/etc/rsyslog/api-keys.txt"
   )

See also
--------
See also :doc:`../../configuration/modules/imhttp`.
