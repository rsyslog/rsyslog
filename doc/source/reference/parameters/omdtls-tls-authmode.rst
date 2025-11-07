.. _param-omdtls-tls-authmode:
.. _omdtls.parameter.input.tls-authmode:

tls.AuthMode
============

.. index::
   single: omdtls; tls.AuthMode
   single: tls.AuthMode

.. summary-start

Sets the DTLS peer authentication method used by the action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: tls.AuthMode
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Sets the mode of authentication to be used.

Supported values are either "*fingerprint*", "*name*" or "*certvalid*".

* **fingerprint**: Authentication based on certificate fingerprint.
* **name**: Authentication based on the ``subjectAltName`` and, as a fallback,
  the subject common name.
* **certvalid**: Requires a valid certificate for authentication.

If this parameter is not set, or if an unsupported value is provided, the
action falls back to anonymous authentication (no client certificate
required).

Input usage
-----------
.. _param-omdtls-input-tls-authmode:
.. _omdtls.parameter.input.tls-authmode-usage:

.. code-block:: rsyslog

   action(type="omdtls" target="192.0.2.1" tls.authMode="certvalid")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
