.. meta::
   :description: Configure imhttp to protect an input with HTTP Basic Authentication.
   :keywords: rsyslog, imhttp, basicauthfile, http basic authentication

.. _param-imhttp-basicauthfile:
.. _imhttp.parameter.input.basicauthfile:
.. _imhttp-basicauthfile:

basicAuthFile
=============

.. index::
   single: imhttp; basicAuthFile
   single: basicAuthFile

.. summary-start

Protects an imhttp input with HTTP Basic Authentication using an htpasswd file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imhttp`.

:Name: basicAuthFile
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: Not specified

Description
-----------
imhttp accepts ``Authorization: Basic`` credentials for the input when this
parameter is set. The htpasswd file is read line-by-line and blank lines or
``#`` comments are ignored.

For safety, the Basic-auth header is capped at 2048 Base64 characters. That is
already larger than practical Basic-auth credentials, but small enough to keep
the parser on a fixed stack buffer and reject oversized headers outright.

Input usage
-----------
.. _imhttp.parameter.input.basicauthfile-usage:

.. code-block:: rsyslog

   input(
       type="imhttp"
       endpoint="/ingest"
       basicAuthFile="/etc/rsyslog/htpasswd"
   )

See also
--------
See also :doc:`../../configuration/modules/imhttp`.
