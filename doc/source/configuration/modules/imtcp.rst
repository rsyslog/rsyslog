******************************
imtcp: TCP Syslog Input Module
******************************

===========================  ===========================================================================
**Module Name:**             **imtcp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages via TCP. Encryption is
natively provided by selecting the appropriate network stream driver
and can also be provided by using `stunnel <rsyslog_stunnel.html>`_ (an
alternative is the use the `imgssapi <imgssapi.html>`_ module).


Notable Features
================

- :ref:`imtcp-statistic-counter`

The ``imtcp`` module runs on all platforms but is **optimized for Linux** and other systems that 
support **epoll in edge-triggered mode**. While earlier versions of imtcp operated exclusively 
in **single-threaded** mode, starting with **version 8.2504.0**, a **worker pool** is used on 
**epoll-enabled systems**, significantly improving performance.

The **number of worker threads** can be configured to match system requirements.

Starvation Protection
---------------------

A common issue in high-volume logging environments is **starvation**, where a few high-traffic 
sources overwhelm the system. Without protection, a worker may become stuck processing a single 
connection continuously, preventing other clients from being served.

For example, if two worker threads are available and one machine floods the system with data, 
**only one worker remains** to handle all other connections. If multiple sources send large 
amounts of data, **all workers could become monopolized**, preventing other connections from 
being processed.

To mitigate this, **imtcp allows limiting the number of consecutive requests a worker can handle 
per session**. Once the limit is reached, the worker temporarily stops processing that session 
and switches to other active connections. This ensures **fair resource distribution** while 
preventing any single sender from **monopolizing rsyslog’s processing power**.

Even in **single-threaded mode**, a high-volume sender may consume significant resources, but it 
will no longer block all other connections.

Configurable Behavior
---------------------

- The **maximum number of requests per session** before switching to another connection can be 
  adjusted.
- In **epoll mode**, the **number of worker threads** can also be configured. More workers 
  provide better protection against single senders dominating processing.

Monitoring and Performance Insights
-----------------------------------

**Statistics counters** provide insights into key metrics, including starvation prevention. 
These counters are **critical for monitoring system health**, especially in **high-volume 
datacenter deployments**.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Module Parameters
-----------------

AddtlFrameDelimiter
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "-1", "no", "``$InputTCPServerAddtlFrameDelimiter``"

.. versionadded:: 4.3.1

This directive permits to specify an additional frame delimiter for
Multiple receivers may be configured by specifying $InputTCPServerRun
multiple times.


DisableLFDelimiter
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputTCPServerDisableLFDelimiter``"

Industry-standard plain text tcp syslog uses the LF to delimit
syslog frames. However, some users brought up the case that it may be
useful to define a different delimiter and totally disable LF as a
delimiter (the use case named were multi-line messages). This mode is
non-standard and will probably come with a lot of problems. However,
as there is need for it and it is relatively easy to support, we do
so. Be sure to turn this setting to "on" only if you exactly know
what you are doing. You may run into all sorts of troubles, so be
prepared to wrangle with that!


MaxFrameSize
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "200000", "no", "none"

When in octet counted mode, the frame size is given at the beginning
of the message. With this parameter the max size this frame can have
is specified and when the frame gets to large the mode is switched to
octet stuffing.
The max value this parameter can have was specified because otherwise
the integer could become negative and this would result in a
Segmentation Fault. (Max Value = 200000000)


NotifyOnConnectionOpen
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", ""

Instructs imtcp to emit a message if the remote peer closes a
connection.


NotifyOnConnectionOpen
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Instructs imtcp to emit a message if the remote peer opens a
connection.


NotifyOnConnectionClose
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputTCPServerNotifyOnConnectionClose``"

Instructs imtcp to emit a message if the remote peer closes a
connection.



KeepAlive
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputTCPServerKeepAlive``"

Enable or disable keep-alive packets at the tcp socket layer. The
default is to disable them.


keepalive.probes
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputTCPServerKeepAlive_probes``"

The number of unacknowledged probes to send before considering the
connection dead and notifying the application layer. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.


keepalive.time
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputTCPServerKeepAlive_time``"

The interval between the last data packet sent (simple ACKs are not
considered data) and the first keepalive probe; after the connection
is marked to need keepalive, this counter is not used any further.
The default, 0, means that the operating system defaults are used.
This has only effect if keep-alive is enabled. The functionality may
not be available on all platforms.


keepalive.interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", ""

.. versionadded:: 8.2106.0

The interval for keep alive packets.




FlowControl
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$InputTCPFlowControl``"

This setting specifies whether some message flow control shall be
exercised on the related TCP input. If set to on, messages are
handled as "light delayable", which means the sender is throttled a
bit when the queue becomes near-full. This is done in order to
preserve some queue space for inputs that can not throttle (like
UDP), but it may have some undesired effect in some configurations.
Still, we consider this as a useful setting and thus it is the
default. To turn the handling off, simply configure that explicitly.


MaxListeners
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "20", "no", "``$InputTCPMaxListeners``"

Sets the maximum number of listeners (server ports) supported.
This must be set before the first $InputTCPServerRun directive.


MaxSessions
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "200", "no", "``$InputTCPMaxSessions``"

Sets the maximum number of sessions supported. This must be set
before the first $InputTCPServerRun directive.


StreamDriver.Name
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Selects :doc:`network stream driver <../../concepts/netstrm_drvr>`
for all inputs using this module.


WorkerThreads
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2", "no", "none"

Introduced in version 8.2504.0.

The ``WorkerThreads`` parameter defines the **default number of worker threads** for all ``imtcp``  
listeners. This setting applies only on **epoll-enabled systems**. If ``epoll`` is unavailable,  
``imtcp`` will always run in **single-threaded mode**, regardless of this setting.

**Default value:** ``2``  
**Allowed values:** ``1`` (single-threaded) to any reasonable number (should not exceed CPU cores).  

**Behavior and Recommendations**

- If set to ``1``, ``imtcp`` operates in **single-threaded mode**, using the main event loop  
  for processing.
- If set to ``2`` or more, a **worker pool** is created, allowing multiple connections to be  
  processed in parallel.
- Setting this too high **can degrade performance** due to excessive thread switching.
- A reasonable upper limit is **the number of available CPU cores**.

**Scope and Overrides**
- This is a **module-level parameter**, meaning it **sets the default** for all ``imtcp`` listeners.
- Each listener instance can override this by setting the ``workerthreads`` **listener parameter**.

**Example Configuration**
The following sets a default of **4** worker threads for all listeners, while overriding it to  
**8** for a specific listener:

.. code-block:: none

    module(load="imtcp" WorkerThreads="4")  # Default for all listeners

    input(type="imtcp" port="514" workerthreads="8")  # Overrides default, using 8 workers

If ``WorkerThreads`` is not explicitly set, the default of ``2`` will be used.


.. _imtcp-StarvationProtection-MaxReads:

StarvationProtection.MaxReads
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "500", "no", "none"


Introduced in version 8.2504.0.

The ``StarvationProtection.MaxReads`` parameter defines the **maximum number of consecutive  
requests** a worker can process for a single connection before switching to another session.  
This mechanism prevents any single sender from **monopolizing imtcp's processing capacity**.

**Default value:** ``500``  

**Allowed values:**  

- ``0`` → Disables starvation protection (a single sender may dominate worker time).  
- Any positive integer → Specifies the maximum number of consecutive reads before switching.  

**Behavior and Use Cases**

- When a connection continuously sends data, a worker will process it **up to MaxReads times**  
  before returning it to the processing queue.
- This ensures that **other active connections** get a chance to be processed.
- Particularly useful in **high-volume environments** where a few senders might otherwise  
  consume all resources.
- In **single-threaded mode**, this still provides fairness but cannot fully prevent resource  
  exhaustion.

**Scope and Overrides**

- This is a **module-level parameter**, meaning it **sets the default** for all ``imtcp`` listeners.
- Each listener instance can override this by setting the  
  ``starvationProtection.maxReads`` **listener parameter**.

**Example Configuration**

The following sets a **default of 300** reads per session before switching to another connection,  
while overriding it to **1000** for a specific listener:

.. code-block:: none

    module(load="imtcp" StarvationProtection.MaxReads="300")  # Default for all listeners

    input(type="imtcp" port="514" starvationProtection.MaxReads="1000")  # Overrides default

If ``StarvationProtection.MaxReads`` is not explicitly set, the default of ``500`` will be used.

StreamDriver.Mode
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputTCPServerStreamDriverMode``"

Sets the driver mode for the currently selected
:doc:`network stream driver <../../concepts/netstrm_drvr>`.
<number> is driver specific.


StreamDriver.AuthMode
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$InputTCPServerStreamDriverAuthMode``"

Sets stream driver authentication mode. Possible values and meaning
depend on the
:doc:`network stream driver <../../concepts/netstrm_drvr>`.
used.


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

.. note::

   The GnuTLS driver sends all certificates contained in the file
   specified via ``StreamDriver.CertFile`` (or
   ``$DefaultNetstreamDriverCertFile``) to connecting clients.  To
   expose intermediate certificates, the file must contain the server
   certificate first, followed by the intermediate certificates.
   This capability was added in rsyslog version 8.36.0.


PermittedPeer
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "``$InputTCPServerStreamDriverPermittedPeer``"

Sets permitted peer IDs. Only these peers are able to connect to
the listener. <id-string> semantics depend on the currently
selected AuthMode and
:doc:`network stream driver <../../concepts/netstrm_drvr>`.
PermittedPeer may not be set in anonymous modes. PermittedPeer may
be set either to a single peer or an array of peers either of type
IP or name, depending on the tls certificate.

Single peer:
PermittedPeer="127.0.0.1"

Array of peers:
PermittedPeer=["test1.example.net","10.1.2.3","test2.example.net","..."]


DiscardTruncatedMsg
^^^^^^^^^^^^^^^^^^^

Normally when a message is truncated in octet stuffing mode the part that
is cut off is processed as the next message. When this parameter is activated,
the part that is cut off after a truncation is discarded and not processed.

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"


gnutlsPriorityString
^^^^^^^^^^^^^^^^^^^^

The "gnutls priority string" parameter in rsyslog offers enhanced
customization for secure communications, allowing detailed configuration
of TLS driver properties. This includes specifying handshake algorithms
and other settings for GnuTLS, as well as implementing OpenSSL
configuration commands. Initially developed for GnuTLS, the "gnutls
priority string" has evolved since version v8.1905.0 to also support
OpenSSL, broadening its application and utility in network security
configurations. This update signifies a key advancement in rsyslog's
capabilities, making the "gnutls priority string" an essential
feature for advanced TLS configuration.

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.29.0


**Configuring Driver-Specific Properties**

This configuration string is used to set properties specific to different drivers. Originally designed for the GnuTLS driver, it has been extended to support OpenSSL configuration commands from version v8.1905.0 onwards.

**GNUTLS Configuration**

In GNUTLS, this setting determines the handshake algorithms and options for the TLS session. It's designed to allow user overrides of the library's default settings. If you leave this parameter unset (NULL), the system will revert to the default settings. For more detailed information on priority strings in GNUTLS, you can refer to the GnuTLS Priority Strings Documentation available at [GnuTLS Website](https://gnutls.org/manual/html_node/Priority-Strings.html).

**OpenSSL Configuration**

This feature is compatible with OpenSSL Version 1.0.2 and above. It enables the passing of configuration commands to the OpenSSL library. You can find a comprehensive list of commands and their acceptable values in the OpenSSL Documentation, accessible at [OpenSSL Documentation](https://docs.openssl.org/1.0.2/man3/SSL_CONF_cmd/).

**General Configuration Guidelines**

The configuration can be formatted as a single line or across multiple lines. Each command within the configuration is separated by a linefeed (`\n`). To differentiate between a command and its corresponding value, use an equal sign (`=`). Below are some examples to guide you in formatting these commands.


Example 1
---------

This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: none

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3"


Example 2
---------

This will allow all protocols except for SSLv2, SSLv3 and TLSv1.
It will also set the minimum protocol to TLSv1.2

.. code-block:: none

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1
   MinProtocol=TLSv1.2"


PreserveCase
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "on", "no", "none"

.. versionadded:: 8.37.0

This parameter is for controlling the case in fromhost.  If preservecase is set to "off", the case in fromhost is not preserved.  E.g., 'host1.example.org' the message was received from 'Host1.Example.Org'.  Default to "on" for the backward compatibility.


Input Parameters
----------------

Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "``$InputTCPServerRun``"

Starts a TCP server on selected port. If port zero is selected, the OS automatically
assigns a free port. Use `listenPortFileName` in this case to obtain the information
of which port was assigned.


ListenPortFileName
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

This parameter specifies a file name into which the port number this input listens
on is written. It is primarily intended for cases when `port` is set to 0 to let
the OS automatically assign a free port number.


Address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

On multi-homed machines, specifies to which local address the
listener should be bound.


Name
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "imtcp", "no", "``$InputTCPServerInputName``"

Sets a name for the inputname property. If no name is set "imtcp" is
used by default. Setting a name is not strictly necessary, but can be
useful to apply filtering based on which input the message was
received from.


Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$InputTCPServerBindRuleset``"

Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.


SupportOctetCountedFraming
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$InputTCPServerSupportOctetCountedFraming``"

If set to "on", the legacy octed-counted framing (similar to RFC5425
framing) is activated. This should be left unchanged until you know
very well what you do. It may be useful to turn it off, if you know
this framing is not used and some senders emit multi-line messages
into the message stream.


SocketBacklog
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10% of configured connections", "no", "none"

Specifies the backlog parameter passed to the `listen()` system call. This parameter
defines the maximum length of the queue for pending connections, which includes
partially established connections (those in the SYN-ACK handshake phase) and fully
established connections waiting to be accepted by the application.

**Available starting with the 8.2502.0 series.**

For more details, refer to the `listen(2)` man page.

By default, the value is set to 10% of the configured connections
to accommodate modern workloads. It can
be adjusted to suit specific requirements, such as:

- **High rates of concurrent connection attempts**: Increasing this value helps handle bursts of incoming connections without dropping them.
- **Test environments with connection flooding**: Larger values are recommended to prevent SYN queue overflow.
- **Servers with low traffic**: Lower values may be used to reduce memory usage.

The effective backlog size is influenced by system-wide kernel settings, particularly `net.core.somaxconn` and `net.ipv4.tcp_max_syn_backlog`. The smaller value between this parameter and the kernel limits is used as the actual backlog.


RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Specifies the rate-limiting interval in seconds. Default value is 0,
which turns off rate limiting. Set it to a number of seconds (5
recommended) to activate rate-limiting.


RateLimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10000", "no", "none"

Specifies the rate-limiting burst in number of messages. Default is
10,000.


listenPortFileName
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.38.0

With this parameter you can specify the name for a file. In this file the
port, imtcp is connected to, will be written.
This parameter was introduced because the testbench works with dynamic ports.

.. note::

   If this parameter is set, 0 will be accepted as the port. Otherwise it
   is automatically changed to port 514


StreamDriver.Name
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


StreamDriver.Mode
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", "``$InputTCPServerStreamDriverMode``"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


StreamDriver.AuthMode
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "module parameter", "no", "``$InputTCPServerStreamDriverAuthMode``"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


StreamDriver.PermitExpiredCerts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


StreamDriver.CheckExtendedKeyPurpose
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


StreamDriver.PrioritizeSAN
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


StreamDriver.TlsVerifyDepth
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


streamDriver.CAFile
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "global parameter", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the DefaultNetstreamDriverCAFile global parameter on the input()
level. For further details, see the global parameter.

streamDriver.CRLFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "optional", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "global parameter", "no", "none"

.. versionadded:: 8.2308.0

This permits to override the CRL (Certificate revocation list) file set via `global()` config
object at the per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.

streamDriver.KeyFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "global parameter", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the DefaultNetstreamDriverKeyFile global parameter on the input()
level. For further details, see the global parameter.


streamDriver.CertFile
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "global parameter", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the DefaultNetstreamDriverCertFile global parameter on the input()
level. For further details, see the global parameter.


PermittedPeer
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "equally-named module parameter"
.. versionadded:: 8.2112.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


gnutlsPriorityString
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "module parameter", "no", "none"
.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


MaxSessions
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


MaxListeners
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


FlowControl
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


DisableLFDelimiter
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "module parameter", "no", ""


.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


DiscardTruncatedMsg
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


NotifyOnConnectionClose
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


AddtlFrameDelimiter
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


MaxFrameSize
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


PreserveCase
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "module parameter", "no", "none"

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


KeepAlive
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


keepalive.probes
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


keepalive.time
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.


keepalive.interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "module parameter", "no", ""

.. versionadded:: 8.2106.0

This permits to override the equally-named module parameter on the input()
level. For further details, see the module parameter.



.. _imtcp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener. The statistic is named
after the given input name (or "imtcp" if none is configured), followed by
the listener port in parenthesis. For example, the counter for a listener
on port 514 with no set name is called "imtcp(514)".

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup


.. _imtcp-worker-statistics:

Worker Statistics Counters
--------------------------

When ``imtcp`` operates with **multiple worker threads** (``workerthreads > 1``),  
it **automatically generates statistics counters** to provide insight into worker  
activity and system health. These counters are part of the ``impstats`` module and  
can be used to monitor system performance, detect bottlenecks, and analyze load  
distribution among worker threads.

**Note:** These counters **do not exist** if ``workerthreads`` is set to ``1``,  
as ``imtcp`` runs in single-threaded mode in that case.

**Statistics Counters**

Each worker thread reports its statistics using the format ``tcpsrv/wX``,  
where ``X`` is the worker thread number (e.g., ``tcpsrv/w0`` for the first worker).  
The following counters are available:

- **runs** → Number of times the worker thread has been invoked.
- **read** → Number of read calls performed by the worker.  
  - For TLS connections, this includes both **read** and **write** calls.
- **accept** → Number of times this worker has processed a new connection via ``accept()``.
- **starvation_protect** → Number of times a socket was placed back into the queue  
  due to reaching the ``StarvationProtection.MaxReads`` limit.

**Example Output**
An example of ``impstats`` output with three worker threads:

.. code-block:: none

    10 Thu Feb 27 16:40:02 2025: tcpsrv/w0: origin=imtcp runs=72 read=2662 starvation_protect=1 accept=2
    11 Thu Feb 27 16:40:02 2025: tcpsrv/w1: origin=imtcp runs=74 read=2676 starvation_protect=2 accept=0
    12 Thu Feb 27 16:40:02 2025: tcpsrv/w2: origin=imtcp runs=72 read=1610 starvation_protect=0 accept=0

In this case:

- Worker ``w0`` was invoked **72 times**, performed **2662 reads**,  
  applied **starvation protection once**, and accepted **2 connections**.
- Worker ``w1`` handled more reads but did not process any ``accept()`` calls.
- Worker ``w2`` processed fewer reads and did not trigger starvation protection.

**Usage and Monitoring**

- These counters help analyze how load is distributed across worker threads.
- High ``starvation_protect`` values indicate that some connections are consuming  
  too many reads, potentially impacting fairness.
- If a single worker handles **most** of the ``accept()`` calls, this may  
  indicate an imbalance in connection handling.
- Regular monitoring can help optimize the ``workerthreads`` and  
  ``StarvationProtection.MaxReads`` parameters for better system efficiency.

By using these statistics, administrators can fine-tune ``imtcp`` to ensure  
**fair resource distribution and optimal performance** in high-traffic environments.


Caveats/Known Bugs
==================

-  module always binds to all interfaces
-  can not be loaded together with `imgssapi <imgssapi.html>`_ (which
   includes the functionality of imtcp)


Examples
========

Example 1
---------

This sets up a TCP server on port 514 and permits it to accept up to 500
connections:

.. code-block:: none

   module(load="imtcp" MaxSessions="500")
   input(type="imtcp" port="514")


Note that the global parameters (here: max sessions) need to be set when
the module is loaded. Otherwise, the parameters will not apply.


Additional Resources
====================

- `rsyslog video tutorial on how to store remote messages in a separate file <http://www.rsyslog.com/howto-store-remote-messages-in-a-separate-file/>`_ (for legacy syntax, but you get the idea).

