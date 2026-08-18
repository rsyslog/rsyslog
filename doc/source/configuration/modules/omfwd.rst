.. _module-omfwd:

**************************************
omfwd: syslog Forwarding Output Module
**************************************

.. index:: ! omfwd

.. meta::
   :description: Forward syslog messages to remote targets over UDP, TCP, or TLS, including DNS SRV discovery.
   :keywords: rsyslog, omfwd, forwarding, syslog, tcp, udp, tls, srv

.. summary-start

omfwd forwards syslog messages to remote systems via UDP, TCP, or TLS, with optional DNS SRV discovery for target pools.

.. summary-end

===========================  ==========================================================
**Module Name:**             **omfwd**
**Author/Maintainer:**       `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Introduced:**              Legacy (pre-3.0)
===========================  ==========================================================

The `omfwd` module forwards logs to remote systems using **UDP, TCP, or TLS**.

It is **built-in** and does not require explicit loading.  
To configure global defaults, use ``builtin:omfwd``.

.. note::
   For modern deployments, prefer **TCP with TLS** over plain TCP or UDP.
   If reliable delivery is critical, consider :doc:`omrelp <omrelp>`.

.. note::
   Hostnames in ``target`` are resolved on each connection attempt using the
   system resolver. Reverse lookup cache settings (:ref:`reverse_dns_cache`)
   do not affect outbound name resolution.

.. note::
   When ``StreamDriver="ossl"`` is used on supported OpenSSL 3 builds,
   ``StreamDriver.CAFile``, ``StreamDriver.CAExtraFiles``,
   ``StreamDriver.CertFile``, and ``StreamDriver.KeyFile`` on either the
   ``omfwd`` action or the ``builtin:omfwd`` module may be configured either
   as normal filesystem paths or as strict ``pkcs11:`` URIs. This allows TLS
   trust anchors, additional intermediate CA certificates, certificates, and
   private keys to be loaded from provider-backed PKCS#11 objects instead of
   PEM files on disk. The global default parameters
   ``DefaultNetstreamDriverCAFile``, ``DefaultNetstreamDriverCertFile``,
   ``DefaultNetstreamDriverKeyFile``, and ``NetstreamDriverCAExtraFiles``
   still validate as filesystem paths today and therefore must not be set to
   ``pkcs11:`` URIs.

Best Practices
==============

- **Choose the right transport:**  
  TCP is often used for reliable delivery, especially with TLS.  
  UDP can be preferable for low-latency, non-blocking scenarios (e.g., local 
  high-speed networks).  
  If you need **encrypted UDP**, consider :doc:`omdtls <omdtls>`.

- **Use TLS where possible:**  
  When sending logs over untrusted networks, configure TLS with TCP by setting a TLS-capable
  driver (``StreamDriver="ossl"``, ``StreamDriver="gtls"`` or ``StreamDriver="mbedtls"``)
  **and** ``StreamDriverMode="1"``, or switch to ``omdtls`` (for UDP).

- **Enable queues for TCP forwarding:**  
  Always define a queue (`queue.type="linkedList"`) to avoid blocking if the 
  remote server is unavailable.

- **Adjust queue parameters for heavy traffic:**  
  For high-volume logging or prolonged outages, tune `queue.size` and related 
  parameters to prevent message loss. The defaults are designed for typical workloads.

- **Use templates for structured logs:**  
  Apply templates (e.g., JSON) if the receiver expects a specific log format.

- **Consider RELP for guaranteed delivery:**  
  Use :doc:`omrelp <omrelp>` when 100% reliable delivery and acknowledgments 
  are required.

Quick Start
===========

The most common forwarding examples:

**1. Forward all logs to a remote server via TCP**

.. code-block:: rsyslog

   # Forward all messages to a remote syslog server over TCP
   action(
       type="omfwd"
       target="logs.example.com"   # Remote syslog server
       port="514"                  # Destination port
       protocol="tcp"              # Use TCP (reliable transport)
       queue.type="linkedList"     # Prevent blocking if the remote server is down
   )

**2. Secure log forwarding with TLS**

.. code-block:: rsyslog

   # Forward logs securely using TLS (RFC5425)
   action(
       type="omfwd"
       target="logs.example.com"
       port="6514"                 # Standard port for TLS syslog
       protocol="tcp"
       StreamDriver="ossl"         # OpenSSL for TLS
       StreamDriverMode="1"        # TLS-only mode
       StreamDriverAuthMode="x509/name"
       StreamDriverPermittedPeers="logs.example.com"
   )

**2a. Secure log forwarding with TLS and PKCS#11 URIs**

.. note::
   The ``pkcs11:`` values shown in the examples are illustrative. Replace
   ``token=``, ``object=``, and any other URI attributes with values that
   match the objects provisioned in your own PKCS#11 token or device.
   rsyslog recognizes only the strict ``pkcs11:`` prefix and otherwise passes
   the URI through to OpenSSL and the configured PKCS#11 provider. The exact
   URI attributes and matching behavior therefore depend on that provider and
   backend implementation.

.. code-block:: rsyslog

   # Set shared PKCS#11-backed TLS defaults for omfwd actions
   module(
       load="builtin:omfwd"
       StreamDriver.CAFile="pkcs11:token=rsyslog;object=ca-cert;type=cert"
       StreamDriver.CAExtraFiles="pkcs11:token=rsyslog;object=intermediate-ca-cert;type=cert"
       StreamDriver.CertFile="pkcs11:token=rsyslog;object=client-cert;type=cert"
       StreamDriver.KeyFile="pkcs11:token=rsyslog;object=client-key;type=private"
   )

   # Forward logs securely using those defaults, while overriding
   # the additional CA list and permitted peer name for this action
   action(
       type="omfwd"
       target="logs.example.com"
       port="6514"
       protocol="tcp"
       StreamDriver="ossl"
       StreamDriverMode="1"
       StreamDriverAuthMode="x509/name"
       StreamDriver.CAExtraFiles="pkcs11:token=rsyslog;object=region2-intermediate-ca-cert;type=cert"
       StreamDriverPermittedPeers="logs.example.com"
       queue.type="linkedList"
   )

   # A second action may reuse the module defaults unchanged
   action(
       type="omfwd"
       target="logs-backup.example.com"
       port="6514"
       protocol="tcp"
       StreamDriver="ossl"
       StreamDriverMode="1"
       StreamDriverAuthMode="x509/name"
       StreamDriverPermittedPeers="logs-backup.example.com"
   )

**3. Discover receivers via DNS SRV**

.. code-block:: rsyslog

   # Resolve _syslog._tcp.example.com SRV records to build the target pool
   action(
       type="omfwd"
       targetSrv="example.com"
       protocol="tcp"
   )

When ``targetSrv`` is set, omfwd queries the protocol-specific SRV record and
constructs the forwarding targets from the returned host and port values. The
SRV priority/weight ordering is preserved before the existing pool/load-balancer
logic runs.

SRV discovery depends on resolver support for ``ns_initparse``. When that
capability is missing at build time, ``targetSrv`` configuration fails with a
not-implemented error.

For test and controlled environments, you can override the resolver used for
SRV lookups by setting ``RSYSLOG_DNS_SERVER`` (IPv4 address) and the optional
``RSYSLOG_DNS_PORT`` environment variables before starting rsyslog.

**4. Stream-compress forwarding to imtcp**

.. code-block:: rsyslog

   # Compress the full TCP stream. Receiver must use matching imtcp settings.
   action(
       type="omfwd"
       target="logs.example.com"
       port="514"
       protocol="tcp"
       compression.mode="stream:always"
       compression.driver="zlib"
       zipLevel="3"
       queue.type="linkedList"
   )

Outdated Legacy Methods
=======================

Many distro defaults and popular online tutorials still use `@server`/`@@server`.
These shorter forms bypass queues and lack back‑pressure control, misleading
users about delivery guarantees and ranking high in search results.
Although longer and seemingly complex, modern RainerScript variants
are explicit, configurable, and robust—preventing hidden pitfalls.

**TCP Forwarding**

**Before (legacy):**

.. code-block:: text

   *.* @@remote.example.com

**After (modern):**

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="remote.example.com"
       port="514"
       protocol="tcp"
       queue.type="linkedList"   # Prevents blocking if remote server is offline
   )

**UDP Forwarding (custom port)**

**Before (legacy):**

.. code-block:: text

   *.* @remote.example.com:515

**After (modern):**

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="remote.example.com"
       port="515"                # Non-standard port
       protocol="udp"
   )

What About `*.*`?
-----------------

The legacy syntax `*.*` means "all facilities and priorities."  
In modern RainerScript, **all messages are matched by default**.  
You only need filters (e.g., `if ... then ...`) when you want to **selectively forward** messages.

.. code-block:: text

   *.* @@remote.example.com

is fully equivalent to the modern example above, **without needing `*.*`.**

Using PKCS#11 Objects with the ossl Driver
==========================================

When ``omfwd`` is configured with ``StreamDriver="ossl"``, rsyslog can load
TLS objects from PKCS#11 URIs instead of from PEM files on the local
filesystem.

This support is useful when you want one or more of the following:

- keep the private key in a hardware security module (HSM), smartcard,
  trusted platform module (TPM)-backed token, or software
  token instead of storing it as a readable PEM file
- use a non-exportable key that OpenSSL may use for signing, but that
  operators and filesystem backups cannot copy out
- centralize certificate and key lifecycle in a device or token manager that
  already speaks PKCS#11
- satisfy operational or compliance requirements that prohibit writing long
  lived private key material to disk

Prerequisites
-------------

Before using PKCS#11 URIs with ``omfwd``, ensure all of the following are true:

- rsyslog is using the ``ossl`` netstream driver
- OpenSSL 3.x is installed and the rsyslog build uses OpenSSL with provider
  support
- an OpenSSL PKCS#11 provider package is installed and configured
- a PKCS#11 backend implementation for your device or token is installed
  (for example, a vendor HSM library or a software token such as SoftHSM)
- the required certificate and private-key objects already exist in the token
- any required login or PIN handling is configured through the provider or the
  runtime environment

rsyslog does not implement PKCS#11 token management itself. It passes the
configured ``pkcs11:`` URI to OpenSSL, and OpenSSL then resolves that URI
through the configured provider and backend module.

For ``omfwd``, configure PKCS#11 URIs either on the action parameters
``StreamDriver.CAFile``, ``StreamDriver.CAExtraFiles``,
``StreamDriver.CertFile``, and ``StreamDriver.KeyFile`` or on the
``builtin:omfwd`` module defaults with the same parameter names. Do not
configure ``pkcs11:`` URIs through ``DefaultNetstreamDriverCAFile``,
``DefaultNetstreamDriverCertFile``, ``DefaultNetstreamDriverKeyFile``, or
``NetstreamDriverCAExtraFiles`` until those global validation paths are
extended to accept non-filesystem objects. ``StreamDriver.CRLFile`` also
remains a filesystem-path setting today.

Action-level Example
--------------------

Use action parameters when only one forwarding action should use PKCS#11
objects:

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="logs.example.com"
       port="6514"
       protocol="tcp"
       StreamDriver="ossl"
       StreamDriverMode="1"
       StreamDriverAuthMode="x509/name"
       StreamDriverPermittedPeers="logs.example.com"
       StreamDriver.CAFile="pkcs11:token=rsyslog;object=ca-cert;type=cert"
       StreamDriver.CAExtraFiles="pkcs11:token=rsyslog;object=intermediate-ca-cert;type=cert"
       StreamDriver.CertFile="pkcs11:token=rsyslog;object=client-cert;type=cert"
       StreamDriver.KeyFile="pkcs11:token=rsyslog;object=client-key;type=private"
       queue.type="linkedList"
   )

Module-default Example
----------------------

Use ``builtin:omfwd`` module defaults when multiple ``omfwd`` actions should
share the same PKCS#11-backed TLS material:

.. code-block:: rsyslog

   module(
       load="builtin:omfwd"
       StreamDriver.CAFile="pkcs11:token=rsyslog;object=ca-cert;type=cert"
       StreamDriver.CAExtraFiles="pkcs11:token=rsyslog;object=intermediate-ca-cert;type=cert"
       StreamDriver.CertFile="pkcs11:token=rsyslog;object=client-cert;type=cert"
       StreamDriver.KeyFile="pkcs11:token=rsyslog;object=client-key;type=private"
   )

   action(
       type="omfwd"
       target="logs.example.com"
       port="6514"
       protocol="tcp"
       StreamDriver="ossl"
       StreamDriverMode="1"
       StreamDriverAuthMode="x509/name"
       StreamDriverPermittedPeers="logs.example.com"
   )

Action-level values override the ``builtin:omfwd`` module defaults for the same
parameter. The module defaults, in turn, override the global netstream driver
defaults. Filesystem-backed module defaults remain available to any TLS-capable
driver, but module defaults whose values are strict ``pkcs11:`` URIs are
inherited only by actions whose effective stream driver is ``ossl``.

Global Defaults
---------------

The global default TLS file parameters remain filesystem-path settings today:

- ``DefaultNetstreamDriverCAFile``
- ``DefaultNetstreamDriverCertFile``
- ``DefaultNetstreamDriverKeyFile``
- ``NetstreamDriverCAExtraFiles``

They are validated during configuration loading with ordinary file access
checks, so they currently cannot be set to ``pkcs11:`` URIs. If multiple
``omfwd`` actions need PKCS#11-backed material, set the PKCS#11 URIs on each
action explicitly or use ``builtin:omfwd`` module defaults.

Operational Tips
----------------

- Prefer provider-based PIN handling over embedding ``pin-value=...`` inside
  the URI in the rsyslog configuration. Provider configuration is usually
  easier to rotate and less likely to be copied into configuration management
  artifacts.
- rsyslog does not interpret provider-specific PKCS#11 URI attributes beyond
  recognizing the strict ``pkcs11:`` prefix. If your environment needs vendor
  attributes, token selectors, or provider-specific matching rules, validate
  them with OpenSSL and the provider first.
- Keep the CA, certificate, and private key aligned. A mismatched certificate
  and key will fail the TLS handshake just as it would with PEM files.
- Use ordinary filesystem paths when PKCS#11 is not needed. PKCS#11 is most
  useful when the private key must remain outside the filesystem or when your
  environment already standardizes on token-backed key management.
- If you use intermediate CA chains, verify how those certificates are
  provisioned in your environment before switching away from PEM bundle files.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Basic Parameters
----------------

.. toctree::
   :maxdepth: 2

   omfwd/parameters/basic
   omfwd/parameters/module

Action Parameters (omfwd)
-------------------------

Action parameters define how a specific forwarding action behaves.  
They apply to each `action(type="omfwd" ...)` statement.

.. note::
   Parameter names are case-insensitive.


pool.resumeinterval
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "30 seconds", "no", "none"

If a target pool exists, "pool.resumeinterval" configures how often an unavailable
target is tried to be activated. A new connection request will be made in roughly
"pool.resumeinterval" seconds until connection is reestablished or the action become
completely suspenden (in which case the action settings take control).

Please note the word "roughly": the interval may be some seconds earlier or later
on a try-by-try basis because of other ongoing activity inside rsyslog.

Warning: we do NOT recommend to set this interval below 10 seconds, as it can lead
DoS-like reconnection behaviour. Actually, the default of 30 seconds is quite short
and should be extended if the use case permits.

NetworkNamespace
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Name of a network namespace in /var/run/netns/ to use for forwarding.

If the setns() system call is unavailable (e.g., BSD kernel, Linux kernel 
before v2.6.24), the given namespace will be ignored.


Address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.35.0

Bind socket to a specific local IP address. This option is supported for 
UDP only, not TCP.


IpFreeBind
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2", "no", "none"

.. versionadded:: 8.35.0

Manages the IP_FREEBIND option on the UDP socket, which allows binding it to
an IP address that is not yet associated to any network interface. This option
is only relevant if the address option is set.

The parameter accepts the following values:

-  0 - does not enable the IP_FREEBIND option on the
   UDP socket. If the *bind()* call fails because of *EADDRNOTAVAIL* error,
   socket initialization fails.

-  1 - silently enables the IP_FREEBIND socket
   option if it is required to successfully bind the socket to a nonlocal address.

-  2 - enables the IP_FREEBIND socket option and
   warns when it is used to successfully bind the socket to a nonlocal address.

Device
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Bind socket to given device (e.g., eth0)

For Linux with VRF support, the Device option can be used to specify the
VRF for the Target address.


TCP_Framing
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "traditional", "no", "none"

Framing mode used for forwarding: either "traditional" or "octet-counted". This 
applies only to TCP-based protocols and is ignored for UDP. In protocol 
engineering, "framing" refers to how multiple messages over the same connection 
are separated. Usually, this is transparent to users. However, the early syslog 
protocol evolved in such a way that users sometimes need to specify the framing.

"Traditional" framing is non-transparent, where messages end when an LF 
(line feed) is encountered, and the next message starts immediately after the 
LF. If multi-line messages are received, they are split into multiple messages, 
with all but the first segment usually incorrectly formatted.

"Octet-counted" framing addresses this issue. Each message is prefixed with its 
length, so the receiver knows exactly where the message ends. Multi-line 
messages are handled correctly. This mode is similar to the method described in 
`RFC5425 <https://datatracker.ietf.org/doc/html/rfc5425>`_ for TLS-enabled 
syslog. Unfortunately, few syslog implementations support "octet-counted" 
framing. As such, "traditional" framing is the default, despite its defects. 
If the receiver supports "octet-counted" framing, it is recommended to use 
that mode.


TCP_FrameDelimiter
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10", "no", "none"

Sets a custom frame delimiter for TCP transmission when running TCP\_Framing
in "traditional" mode. The delimiter has to be a number between 0 and 255
(representing the ASCII-code of said character). The default value for this
parameter is 10, representing a '\\n'. When using Graylog, the parameter
must be set to 0.


ZipLevel
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Compression level for messages.

Up until rsyslog 7.5.1, this was the only compression setting that
rsyslog understood. Starting with 7.5.1, we have different
compression modes. All of them are affected by the ziplevel. If,
however, no mode is explicitly set, setting ziplevel also turns on
"single" compression mode, so pre 7.5.1 configuration will continue
to work as expected.

The compression level is specified via the usual factor of 0 to 9,
with 9 being the strongest compression (taking up most processing
time) and 0 being no compression at all (taking up no extra
processing time).


compression.Mode
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

*mode* is one of "none", "single", or "stream:always". The default
is "none", in which no compression happens at all.
In "single" compression mode, Rsyslog implements a proprietary
capability to zip transmitted messages. That compression happens on a
message-per-message basis. As such, there is a performance gain only
for larger messages. Before compressing a message, rsyslog checks if
there is some gain by compression. If so, the message is sent
compressed. If not, it is sent uncompressed. As such, it is totally
valid that compressed and uncompressed messages are intermixed within
a conversation.

In "stream:always" compression mode the full stream is being
compressed. This also uses non-standard protocol and is compatible
only with receivers configured for the same stream-compression mode
and driver. This mode offers
potentially very high compression ratios. With typical syslog
messages, it can be as high as 95+% compression (so only one
twentieth of data is actually transmitted!). Note that this mode
introduces extra latency, as data is only sent when the compressor
emits new compressed data. For typical syslog messages, this can mean
that some hundred messages may be held in local buffers before they
are actually sent. This mode has been introduced in 7.5.1.

Use :doc:`imtcp <imtcp>` with ``compression.mode="stream:always"`` to receive
stream-compressed TCP data from ``omfwd``. ``imtcp`` does not receive the
sender-side ``single`` compression mode.

Stream compression is an rsyslog layer and is not TLS-native compression. The
default compression driver is ``zlib``. Set ``compression.driver="zstd"`` only
when both the sender and receiver are built with libzstd support and both sides
are configured for ``zstd``. For zstd stream compression, ``zipLevel`` must be
non-zero; libzstd treats level 0 as its default compression level rather than
as "no compression".

Stream compression is intended for trusted company VPNs and internal WAN/LAN
deployments where lower bandwidth usage is valuable and both endpoints are
under the same administrative control. Compression can expose side channels
through compressed sizes and timing, especially when attacker-controlled text
can be placed near sensitive log content. For untrusted networks, use TLS for
confidentiality and authentication and decide separately whether compression is
acceptable for the data.

For long-lived stream-compressed TCP sessions, ``rebindInterval`` can be used
to periodically reconnect after a configured number of batches. This resets the
compression context and can reduce the amount of data exposed through one
continuous compression history, at the cost of reconnect overhead. A future
time-based reconnect option may provide a more direct way to reset compression
context by elapsed time.


compression.driver
^^^^^^^^^^^^^^^^^^

.. include:: ../../reference/parameters/omfwd-compression-driver.rst
   :start-after: .. summary-start
   :end-before: .. summary-end

See :ref:`param-omfwd-compression-driver` for full details.


compression.stream.flushOnTXEnd
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

.. versionadded:: 7.5.3

This setting affects stream compression mode, only. If enabled (the
default), the compression buffer will by emptied at the end of a
rsyslog batch. If set to "off", end of batch will not affect
compression at all.

While setting it to "off" can potentially greatly improve
compression ratio, it will also introduce severe delay between when a
message is being processed by rsyslog and actually sent out to the
network. We have seen cases where for several thousand message not a
single byte was sent. This is good in the sense that it can happen
only if we have a great compression ratio. This is most probably a
very good mode for busy machines which will process several thousand
messages per second and the resulting short delay will not pose any
problems. However, the default is more conservative, while it works
more "naturally" with even low message traffic. Even in flush mode,
notable compression should be achievable (but we do not yet have
practice reports on actual compression ratios).


RebindInterval
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionSendTCPRebindInterval`` or ``$ActionSendUDPRebindInterval``"

Permits to specify an interval at which the current connection is
broken and re-established. This setting is primarily an aid to load
balancers. After the configured number of batches (equals roughly to
messages for UDP traffic, dependent on batch size for TCP) has been
transmitted, the current connection is terminated and a new one
started. Note that this setting applies to both TCP and UDP traffic.
For UDP, the new \`\`connection'' uses a different source port (ports
are cycled and not reused too frequently). This usually is perceived
as a \`\`new connection'' by load balancers, which in turn forward
messages to another physical target system.

For ``compression.mode="stream:always"``, reconnecting also starts a new
compression context. This can be used as a practical mitigation when long-lived
compressed streams are acceptable operationally but should not keep one
compression history indefinitely.


KeepAlive
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Enable or disable keep-alive packets at the tcp socket layer. The
default is to disable them.


KeepAlive.Probes
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The number of unacknowledged probes to send before considering the
connection dead and notifying the application layer. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.


KeepAlive.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The interval between subsequential keepalive probes, regardless of
what the connection has exchanged in the meantime. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.


KeepAlive.Time
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The interval between the last data packet sent (simple ACKs are not
considered data) and the first keepalive probe; after the connection
is marked to need keepalive, this counter is not used any further.
The default, 0, means that the operating system defaults are used.
This has only effect if keep-alive is enabled. The functionality may
not be available on all platforms.

tcp_user_timeout
^^^^^^^^^^^^^^^^

.. include:: ../../reference/parameters/omfwd-tcp-user-timeout.rst
   :start-after: .. summary-start
   :end-before: .. summary-end

ConErrSkip
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The ConErrSkip can be used to limit the number of network errors
recorded in logs. For example, value 10 means that each 10th error
message is logged. Note that this options should be used as the last
resort since the necessity of its use indicates network issues.
The default behavior is that all network errors are logged.

RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "", "no", "none"

Specifies the rate-limiting interval in seconds. Default value is 0,
which turns off rate limiting.

RateLimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "none"
   :widths: auto
   :class: parameter-table

   "integer", "200", "(2^32)-1", "no", "none"

Specifies the rate-limiting burst in number of messages.


RateLimit.Name
^^^^^^^^^^^^^^

.. include:: ../../reference/parameters/omfwd-ratelimit-name.rst
   :start-after: .. summary-start
   :end-before: .. summary-end


StreamDriver
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$ActionSendStreamDriver``"

The recommended alias, compatible with imtcp, is "StreamDriver.Name".

Choose the stream driver to be used. Default is plain tcp, but
you can also choose "ossl" or "gtls" for TLS encryption.

Note: aliases help, but are not a great solution. They may
cause confusion if both names are used together in a single
config. So care must be taken when using an alias.


StreamDriverMode
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionSendStreamDriverMode``"

The recommended alias, compatible with imtcp, is "StreamDriver.Mode".

Mode to use with the stream driver (driver-specific)

Mode ``0`` is plain TCP for the built-in network stream drivers. A TLS-capable
driver name such as ``ossl``, ``gtls``, or ``mbedtls`` does not activate TLS by
itself; set mode ``1`` to use TLS. In
``global(compatibility.defaults.secure="strict")``, an omitted mode is promoted
to ``1`` when the effective stream driver is TLS-capable, while an explicit
mode ``0`` is rejected.

Note: aliases help, but are not a great solution. They may
cause confusion if both names are used together in a single
config. So care must be taken when using an alias.


StreamDriverAuthMode
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$ActionSendStreamDriverAuthMode``"

The recommended alias, compatible with imtcp, is "StreamDriver.AuthMode".

Authentication mode to use with the stream driver. Note that this
parameter requires TLS netstream drivers. For all others, it will be
ignored. (driver-specific).

Note: aliases help, but are not a great solution. They may
cause confusion if both names are used together in a single
config. So care must be taken when using an alias.


StreamDriver.PermitExpiredCerts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "warn", "no", "none"

Controls how expired certificates will be handled when stream driver is in TLS mode.
It can have one of the following values:

-  on = Expired certificates are allowed

-  off = Expired certificates are not allowed  (Default, changed from warn to off since Version 8.2012.0)

-  warn = Expired certificates are allowed but warning will be logged


StreamDriverPermittedPeers
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$ActionSendStreamDriverPermittedPeers``"

Accepted fingerprint (SHA1) or name of remote peer. Note that this
parameter requires TLS netstream drivers. For all others, it will be
ignored. (driver-specific)


StreamDriver.CheckExtendedKeyPurpose
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Whether to check also purpose value in extended fields part of certificate
for compatibility with rsyslog operation. (driver-specific)


StreamDriver.PrioritizeSAN
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Whether to use stricter SAN/CN matching. (driver-specific)

When set to "on", if any SAN is found in the peer certificate, only the SAN is
used for name validation and the CN is ignored (per RFC 6125). If the
certificate contains *no* SAN entries at all, validation falls back to checking
the CN — certificates are not rejected simply for lacking SANs.

This setting only affects name-checking auth modes (``x509/name``). It has no
effect when using ``x509/certvalid``, which does not perform name matching.


StreamDriver.TlsVerifyDepth
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "TLS library default", "no", "none"


Specifies the allowed maximum depth for the certificate chain verification.
Support added in v8.2001.0, supported by GTLS and OpenSSL driver.
If not set, the API default will be used.
For OpenSSL, the default is 100 - see the doc for more:
https://docs.openssl.org/1.1.1/man3/SSL_CTX_set_verify/
For GnuTLS, the default is 5 - see the doc for more:
https://www.gnutls.org/manual/gnutls.html

StreamDriver.CAFile
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the CA file set via `global()` config object at the
per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.

StreamDriver.CAExtraFiles
^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "global() default", "no", "none"

.. versionadded:: 8.2608.0

This permits to override the additional CA certificate list set via the
`global()` config object at the per-action basis. The value is a comma-separated
list of filesystem paths or, for the ``ossl`` stream driver on supported
OpenSSL 3 builds, strict ``pkcs11:`` URIs. This parameter is ignored if the
netstream driver and/or its mode does not need or support certificates.

StreamDriver.CRLFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "optional", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2308.0

This permits to override the CRL (Certificate revocation list) file set via `global()` config
object at the per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.

StreamDriver.KeyFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the CA file set via `global()` config object at the
per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.

StreamDriver.CertFile
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the CA file set via `global()` config object at the
per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.


ResendLastMSGOnReconnect
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$ActionSendResendLastMsgOnReconnect``"

Permits to resend the last message when a connection is reconnected.
This setting affects TCP-based syslog, only. It is most useful for
traditional, plain TCP syslog. Using this protocol, it is not always
possible to know which messages were successfully transmitted to the
receiver when a connection breaks. In many cases, the last message
sent is lost. By switching this setting to "yes", rsyslog will always
retransmit the last message when a connection is reestablished. This
reduces potential message loss, but comes at the price that some
messages may be duplicated (what usually is more acceptable).

Please note that busy systems probably loose more than a
single message in such cases. This is caused by an
`inherant unreliability in plain tcp syslog
<https://rainer.gerhards.net/2008/04/on-unreliability-of-plain-tcp-syslog.html>`_
and there is no way rsyslog could prevent this from happening
(if you read the detail description, be sure to follow the link
to the follow-up posting). In order to prevent these problems,
we recommend the use of :doc:`omrelp<omrelp>`.


udp.SendToAll
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When sending UDP messages, there are potentially multiple paths to
the target destination. By default, rsyslogd
only sends to the first target it can successfully send to. If this
option is set to "on", messages are sent to all targets. This may improve
reliability, but may also cause message duplication. This option
should be enabled only if it is fully understood.

Note: this option replaces the former -A command line option. In
contrast to the -A option, this option must be set once per
input() definition.


udp.SendDelay
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

.. versionadded:: 8.7.0

This is an **expert option**, do only use it if you know very well
why you are using it!

This options permits to introduce a small delay after *each* send
operation. The integer specifies the delay in microseconds. This
option can be used in cases where too-quick sending of UDP messages
causes message loss (UDP is permitted to drop packets if e.g. a device
runs out of buffers). Usually, you do not want this delay. The parameter
was introduced in order to support some testbench tests. Be sure
to think twice before you use it in production.


gnutlsPriorityString
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.29.0

This strings setting is used to configure driver specific properties.
Historically, the setting was only meant for gnutls driver. However
with version v8.1905.0 and higher, the setting can also be used to set openssl configuration commands.

For GNUTls, the setting specifies the TLS session's handshake algorithms and
options. These strings are intended as a user-specified override of the library
defaults. If this parameter is NULL, the default settings are used. More
information about priority Strings
`here <https://gnutls.org/manual/html_node/Priority-Strings.html>`_.

For OpenSSL, the setting can be used to pass configuration commands to openssl library.
OpenSSL Version 1.0.2 or higher is required for this feature.
A list of possible commands and their valid values can be found in the documentation:
https://docs.openssl.org/1.0.2/man3/SSL_CONF_cmd/

Native post-quantum TLS depends on the distro-provided OpenSSL or GnuTLS
version. Rsyslog currently supports native PQ only on newer distro baselines
that already ship the required library support and does not add provider-mode
compatibility for older versions.

The setting can be single or multiline, each configuration command is separated by linefeed (\n).
Command and value are separated by equal sign (=). Here are a few samples:

Example 1
---------

This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: rsyslog

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3"


Example 2
---------

This will allow all protocols except for SSLv2, SSLv3 and TLSv1.
It will also set the minimum protocol to TLSv1.2

.. code-block:: rsyslog

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1
   MinProtocol=TLSv1.2"

Example 3
---------

Native OpenSSL hybrid post-quantum TLS on supported distro versions:

.. code-block:: rsyslog

   gnutlsPriorityString="MinProtocol=TLSv1.3
   MaxProtocol=TLSv1.3
   Groups=X25519MLKEM768"

Example 4
---------

Native GnuTLS hybrid post-quantum TLS on supported distro versions:

.. code-block:: rsyslog

   gnutlsPriorityString="NORMAL:-GROUP-ALL:+GROUP-X25519-MLKEM768:+GROUP-X25519"



extendedConnectionCheck
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "true", "no", "none"

This setting permits to control if rsyslog should try to detect if the remote
syslog server has broken the current TCP connection. It is has no meaning when
UDP protocol is used.

Generally, broken connections are not easily detectable. That setting does additional
API calls to check for them. This causes some extra overhead, but is traditionally
enabled.

Especially in very busy systems it is probably worth to disable it. The extra overhead
is unlikely to bring real benefits in such scenarios.

Note: If you need reliable delivery, do NOT use plain TCP syslog transport.
Use RELP instead.

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each forwarding action.
The statistic is named "target-port-protocol" where "target", "port", and
"protocol" are the respective configuration parameters. So an actual name might be
"192.0.2.1-514-TCP" or "example.net-10514-UDP".

The following properties are maintained for each action:

-  **bytes.sent** - total number of bytes sent to the network
-  **messages.sent** - total number of messages sent to the network
-  **num.connects** - total number of successful TCP/TLS connections established

The ``num.connects`` counter is updated only for connection-oriented
forwarding actions. UDP actions do not establish sessions and therefore do not
increment it.

See Also
========

-  `Encrypted Disk
   Queues <http://www.rsyslog.com/encrypted-disk-queues/>`_


Examples
========

Example 1
---------

The following command sends all syslog messages to a remote server via
TCP port 10514.

.. code-block:: rsyslog

   action(type="omfwd" Target="192.168.2.11" Port="10514" Protocol="tcp" Device="eth0")


Example 2
---------

In case the system in use has multiple (maybe virtual) network interfaces network
namespaces come in handy, each with its own routing table. To be able to distribute
syslogs to remote servers in different namespaces specify them as separate actions.

.. code-block:: rsyslog

   action(type="omfwd" Target="192.168.1.13" Port="10514" Protocol="tcp" NetworkNamespace="ns_eth0.0")
   action(type="omfwd" Target="192.168.2.24" Port="10514" Protocol="tcp" NetworkNamespace="ns_eth0.1")
   action(type="omfwd" Target="192.168.3.38" Port="10514" Protocol="tcp" NetworkNamespace="ns_eth0.2")
