.. SPDX-License-Identifier: Apache-2.0

.. _param-omdtls-tls-authmode:
.. _omdtls.parameter.input.tls-authmode:

tls.authmode
============

.. index::
   single: omdtls; tls.authmode
   single: tls.authmode

.. summary-start

Sets the DTLS peer authentication method used by the action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: tls.authmode
:Scope: action
:Type: string
:Default: anonymous authentication
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Sets the mode of authentication to be used.

Supported values are ``fingerprint``, ``name``, or ``certvalid``.

* ``fingerprint``: The trusted server certificate must match the complete
  SHA-256 fingerprint in ``tls.permittedpeer``. The pin is required in this
  mode and has the form ``SHA256:AA:BB:...`` (32 colon-separated octets).
* ``name``: The trusted server certificate must identify the expected server.
  The expected name is ``tls.permittedpeer`` when set, otherwise ``target``.
  DNS names use ``subjectAltName`` with a common-name fallback when no DNS
  subjectAltName is present. IP addresses require a matching IP subjectAltName.
* ``certvalid``: Requires a valid certificate for authentication.

All three modes require a certificate trusted by the configured CA set.
``certvalid`` does not bind the certificate to ``target``. Use it only when
chain validation alone is the intended policy. Existing ``name`` deployments
that connect by IP to a DNS-named server should set ``tls.permittedpeer`` to
that DNS name; ``fingerprint`` deployments must add an explicit SHA-256 pin.

If this parameter is not set, or if an unsupported value is provided, the
action falls back to anonymous authentication (no client certificate
required).

Input usage
-----------
.. _omdtls.parameter.input.tls-authmode-usage:

.. code-block:: rsyslog

   action(type="omdtls" target="192.0.2.1" port="4433" tls.authMode="certvalid")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
