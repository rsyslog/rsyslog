.. _faq-tls-anon-auth-mitm:
.. _faq.tls.anon.auth.mitm:

Why does rsyslog warn that anonymous TLS authentication allows MITM?
=====================================================================

.. meta::
   :description: Explains why anonymous TLS auth modes do not authenticate peer identity and therefore permit MITM attacks.
   :keywords: rsyslog, tls, authmode anon, streamdriver.authmode anon, tls.authmode anon, mitm

.. summary-start

TLS with anonymous authentication encrypts traffic but does not authenticate peer identity.
Because identity is not verified, an active man-in-the-middle (MITM) can impersonate endpoints.

.. summary-end

What this warning means
-----------------------

The warning appears when TLS is enabled but authentication mode is set to anonymous, for example:

* ``streamdriver.authmode="anon"`` (``imtcp`` / ``omfwd``)
* ``tls.authmode="anon"`` (``imrelp`` / ``omrelp``)

In this mode, you get encryption on the wire, but you do **not** get authenticated peer identity.

Why this matters
----------------

Without identity checks (certificate name or chain validation against expected peers), an attacker
who can intercept traffic can present another endpoint and relay/alter messages. That is exactly the
MITM risk the warning calls out.

How to fix
----------

Use authenticated TLS mode instead of anonymous mode:

* Configure certificate trust (CA/cert/key as required by module/driver).
* Use non-anonymous auth modes (for example x509-based modes) and set permitted peer constraints.

How to turn this warning off
----------------------------

These messages are emitted by compatibility secure mode ``warn``.
You can silence them by changing the global policy:

* ``global(compatibility.defaults.secure="backward-compatible")``:
  keeps old insecure defaults and suppresses these warnings.
* ``global(compatibility.defaults.secure="strict")``:
  enforces secure defaults and also suppresses these warnings because insecure
  defaults are no longer used.

The recommended path is to keep ``warn`` until configuration is remediated,
then move to ``strict``.

Primary tutorials
-----------------

* :doc:`../tutorials/tls` — TLS setup fundamentals
* :doc:`../tutorials/tls_cert_summary` — certificate workflow and trust model
* :doc:`../tutorials/tls_cert_client` and :doc:`../tutorials/tls_cert_server` — practical endpoint configuration

See also
--------

* :doc:`tls_mode0_disables_tls` — TLS parameters present but TLS not active
* :doc:`imtcp-tls-gibberish` — symptom when TLS/plain transport do not match
* :doc:`../configuration/modules/imtcp`
* :doc:`../configuration/modules/omfwd`
* :doc:`../configuration/modules/imrelp`
* :doc:`../configuration/modules/omrelp`
