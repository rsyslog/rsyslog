.. _param-imdtls-tls-authmode:
.. _imdtls.parameter.input.tls-authmode:

tls.AuthMode
============

.. index::
   single: imdtls; tls.AuthMode
   single: tls.AuthMode

.. summary-start

Defines the mutual authentication method used for DTLS clients.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: tls.AuthMode
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Sets the mode used for mutual authentication.

Supported values are:

* **fingerprint**: Authentication based on certificate fingerprint.
* **name**: Authentication based on the ``subjectAltName`` and, as a fallback,
  the ``subject common name``.
* **certvalid**: Requires a valid certificate for authentication.

If any other value is provided, or if the parameter is omitted, anonymous
authentication (**certanon**) is used, which does not require a client
certificate.

Input usage
-----------
.. _imdtls.parameter.input.tls-authmode-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" tls.authMode="certvalid")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
