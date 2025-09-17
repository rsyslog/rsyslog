.. _faq-imtcp-tls-gibberish:
.. _faq.tls.gibberish:

Why do I see gibberish when connecting with TLS?
================================================

.. meta::
   :keywords: rsyslog, imtcp, imptcp, TLS, gibberish logs, ClientHello, handshake error, syslog TLS mismatch

.. summary-start

If a TLS client connects to a plain TCP :doc:`imtcp <../configuration/modules/imtcp>` or
:doc:`imptcp <../configuration/modules/imptcp>` listener, the handshake is logged as unreadable
binary gibberish instead of syslog messages.

.. summary-end

Description
-----------

This problem occurs when a sender enables TLS, but the receiving
:doc:`imtcp <../configuration/modules/imtcp>` or
:doc:`imptcp <../configuration/modules/imptcp>` input is still configured for plain TCP.
The TLS handshake (starting with a ClientHello) is delivered as binary data, which shows
up in the logs as blocks of unreadable characters such as::

   16 17:48:06 #26#003#001#001...

This is not corruption of syslog messages, but simply the TLS handshake being
interpreted as text.

Detection strategy
------------------

Starting with version 8.2510, `imtcp` inspects the first few
bytes from new connections. If it sees a TLS handshake pattern, it emits an
explicit warning such as::

   imtcp: TLS handshake detected from sender.example.com (192.0.2.10:65123) but
   listener is not TLS-enabled. Enable TLS on this listener or disable TLS on
   the client. See https://www.rsyslog.com/doc/faq/imtcp-tls-gibberish.html

The probe checks for:

* Record type ``0x16`` (TLS handshake)
* Protocol major version ``0x03`` with minor ``0x00``–``0x04`` (SSLv3, TLS 1.0–1.3)
* A plausible record length (40–16384 bytes)

If these criteria match and the listener is plain TCP (``streamDriver.mode="0"``),
``imtcp`` raises the warning but continues to process the connection.

Versions prior to 8.2510 did not detect TLS handshake attempts and simply
accepted the gibberish as syslog messages. As of late 2025, most
distro-shipped packages still use these older versions.

For :doc:`imptcp <../configuration/modules/imptcp>`, no explicit error message is
generated. Since this module does not support TLS at all, TLS handshakes will
always appear as gibberish if a client attempts to use TLS.

Resolution
----------

Choose one of these fixes so both client and server agree on transport settings:

* **Enable TLS on the listener.** Configure a TLS-capable stream driver and set
  ``streamDriver.mode="1"`` (or another TLS mode), for example::

     module(load="imtcp" streamDriver.name="gtls" streamDriver.mode="1")
     input(type="imtcp" port="6514" streamDriver.authmode="x509/name"
           streamDriver.permittedPeer="client.example.com"
           streamDriver.certFile="/etc/rsyslog.d/server-cert.pem"
           streamDriver.keyFile="/etc/rsyslog.d/server-key.pem"
           streamDriver.caFile="/etc/rsyslog.d/ca.pem")

* **Disable TLS on the sender.** If encryption is not required, reconfigure the
  client to send plain TCP.

After updating configuration, restart the affected client (and server if TLS was
enabled). The unreadable gibberish will disappear once both sides negotiate the
same transport mode.

Notes
-----

* Case-insensitive module and parameter names are supported, but prefer
  CamelCase in modern configs.
* Older deployments may silently drop TLS attempts without warning. Upgrade to a
  newer rsyslog version to benefit from explicit mismatch detection.

See also
--------

* :doc:`../concepts/netstrm_drvr` — overview of TLS options in rsyslog
* :doc:`../configuration/modules/imtcp` — reference for the TCP input module
* :doc:`../configuration/modules/imptcp` — reference for the imptcp input module
* :doc:`../troubleshooting/index` — general debugging tips
