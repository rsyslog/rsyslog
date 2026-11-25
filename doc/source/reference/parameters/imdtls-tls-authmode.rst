.. SPDX-License-Identifier: Apache-2.0

.. _param-imdtls-tls-authmode:
.. _imdtls.parameter.input.tls-authmode:

tls.authMode
============

.. index::
   single: imdtls; tls.authMode
   single: tls.authMode

.. summary-start

Defines the mutual authentication method used for DTLS clients.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: tls.authMode
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

* **anon** or **certanon**: Anonymous authentication, which does not require
  a client certificate. If the parameter is omitted for an input, the
  module-level ``tls.authmode`` value is used (anonymous by default).

Any other value is a configuration error when
``global(compatibility.defaults.secure="strict")`` is set: the input (or, for
the module parameter, the module configuration) is rejected. With the
``warn`` (default) and ``backward-compatible`` policies, rsyslog keeps the
historical behavior for compatibility: it logs a warning and uses anonymous
authentication, so the peer is **not** verified. Check startup warnings for
typos in this value.

Input usage
-----------
.. _imdtls.parameter.input.tls-authmode-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" tls.authMode="certvalid")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
