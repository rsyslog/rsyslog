.. _faq-tls-mode0-disables-tls:
.. _faq.tls.mode0.disables.tls:

Why does rsyslog warn that TLS is not active with streamdriver.mode="0"?
=========================================================================

.. meta::
   :description: Explains why TLS settings are ignored when streamdriver.mode=0 or when a transport runs without TLS.
   :keywords: rsyslog, tls, streamdriver.mode, mode 0, plain tcp, omfwd, imtcp, imrelp, omrelp

.. summary-start

If a connection is configured with plain transport (for example, ``streamdriver.mode="0"``,
UDP, or RELP ``tls="off"``), TLS is not in use even if other TLS-related parameters are present.

.. summary-end

What this warning means
-----------------------

The warning indicates that the effective transport is not TLS-protected. Common cases are:

* ``imtcp`` or ``omfwd`` with ``streamdriver.mode="0"`` (plain TCP)
* ``imtcp`` or ``omfwd`` using a TLS-capable stream driver such as ``ossl``,
  ``gtls``, or ``mbedtls`` while the mode is still ``0``; this includes drivers
  inherited from ``global(defaultNetstreamDriver="...")``
* ``omfwd`` with ``protocol="udp"``
* ``imrelp`` / ``omrelp`` with ``tls="off"``

When these settings are active, encryption and certificate checks do not happen for that path.

Why this matters
----------------

If operators set TLS-related parameters (driver name, auth mode, certs) but transport mode still
selects plain communication, configuration can look secure while traffic remains unencrypted.
The warning is emitted to prevent this false sense of security.

How to fix
----------

Pick one explicit model and make it consistent:

* **Use TLS intentionally:** configure a TLS-capable stream driver and set
  ``streamdriver.mode="1"`` / ``StreamDriverMode="1"``. Selecting
  ``StreamDriver="ossl"``, ``"gtls"``, or ``"mbedtls"`` chooses the driver,
  but mode ``1`` is what activates TLS.
* **Use plain transport intentionally:** remove TLS-only parameters so intent is clear and warnings stop.

How to turn this warning off
----------------------------

These messages are emitted by compatibility secure mode ``warn``.
You can silence them by changing the global policy:

* ``global(compatibility.defaults.secure="backward-compatible")``:
  keeps old insecure defaults and suppresses these warnings.
* ``global(compatibility.defaults.secure="strict")``:
  promotes an omitted stream-driver mode to TLS mode ``1`` when the effective
  stream driver is TLS-capable. An explicit ``streamdriver.mode="0"`` with a
  TLS-capable effective driver is rejected so that strict mode does not
  silently override explicit plain-TCP intent.

The recommended path is to keep ``warn`` until configuration is remediated,
then move to ``strict``.

Primary tutorials
-----------------

* :doc:`../tutorials/tls` — end-to-end TLS setup basics
* :doc:`../tutorials/tls_cert_summary` — certificate-based TLS deployment flow
* :doc:`../tutorials/reliable_forwarding` — forwarding patterns you can combine with TLS

See also
--------

* :doc:`tls_anon_auth_mitm` — why anonymous TLS auth still permits MITM
* :doc:`imtcp-tls-gibberish` — TLS client talking to a plain listener
* :doc:`../configuration/modules/imtcp`
* :doc:`../configuration/modules/omfwd`
* :doc:`../configuration/modules/imrelp`
* :doc:`../configuration/modules/omrelp`
