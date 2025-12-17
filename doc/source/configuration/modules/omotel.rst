.. _module-omotel:

.. meta::
   :description: Scaffolding for the omotel OpenTelemetry output module.
   :keywords: omotel, otlp, opentelemetry, rsyslog module

.. summary-start

Phase 1 of the omotel output plugin streams OpenTelemetry logs over
OTLP/HTTP JSON with configurable batching, gzip compression, retry/backoff
controls, TLS/mTLS support for secure HTTPS connections, and HTTP proxy
support for corporate networks and firewalled environments.

.. summary-end

omotel: OpenTelemetry output module (preview)
=============================================

.. warning::

   The OTLP/HTTP JSON transport is ready for preview deployments. The optional
   OTLP/gRPC façade and HTTP/protobuf fast-path remain under development, so
   keep action queues enabled and plan future upgrades accordingly.

Overview
--------

``omotel`` prepares rsyslog for native :abbr:`OTLP (OpenTelemetry Log Protocol)`
exports. Phase 1 focuses on the OTLP/HTTP JSON transport path: the module maps
rsyslog metadata into the canonical OTLP JSON structure, joins the configured
endpoint and path, and posts batches of rendered payloads via ``libcurl`` using
``application/json`` semantics. Batching thresholds, gzip compression, retry and
backoff policies, custom headers, TLS/mTLS authentication, and HTTP proxy support
are all configurable. Subsequent phases will extend the module with the gRPC
façade and, optionally, HTTP/protobuf support.

Availability
------------

The module is built only when ``./configure`` is invoked with
``--enable-omotel=yes`` and both ``libcurl`` and ``libfastjson`` are present.
The default ``--enable-omotel`` setting is ``no``, so you must opt in
explicitly. The HTTP transport depends on ``libcurl`` at runtime.

Configuration
-------------

The action parameters listed below mirror the current transport design. All
parameters are optional and fall back to sensible defaults inspired by the
`OTLP specification <https://opentelemetry.io/docs/specs/otlp/>`_.

.. csv-table:: ``omotel`` action parameters
   :header: "Parameter", "Type", "Default", "Description"
   :widths: auto

   "endpoint", "string", "http://127.0.0.1:4318", "Base OTLP collector URL"
   "path", "string", "/v1/logs", "Request path appended to the endpoint"
   "protocol", "word", "http/json", "Transport variant"
   "template", "word", "RSYSLOG_FileFormat", "Message template used for the log body"
   "timeout.ms", "integer", "10000", "HTTP request timeout in milliseconds"
   "compression", "word", "none", "Request payload compression (``none`` or ``gzip``)"
   "batch.max_items", "integer", "512", "Flush the batch after this many records (``0`` disables the limit)"
   "batch.max_bytes", "integer", "524288", "Approximate uncompressed payload threshold in bytes (``0`` disables the limit)"
   "batch.timeout.ms", "integer", "5000", "Flush the oldest batch after this idle period in milliseconds (``0`` disables the user-configured timer, but a short safety flush still runs periodically to avoid indefinite buffering)"
   "retry.initial.ms", "integer", "1000", "Initial backoff before retrying retryable HTTP responses"
   "retry.max.ms", "integer", "30000", "Maximum backoff applied between retries"
   "retry.max_retries", "integer", "5", "Attempts made before returning the batch to the action queue"
   "retry.jitter.percent", "integer", "20", "Jitter applied to retry delays (``0``–``100``)"
   "headers", "string (JSON object)", "—", "Additional HTTP headers expressed as a JSON object"
   "bearer_token", "string", "—", "Convenience token that expands to ``Authorization: Bearer <token>``"
   "resource", "string (JSON)", "—", "Full resource attributes as JSON object. Merged with automatic attributes (service.name, telemetry.sdk.*). Supports string, integer, and boolean values (boolean exported as OTLP boolValue)."
   "resource.service_instance_id", "string", "—", "Legacy: service.instance.id attribute (deprecated, use resource JSON)"
   "resource.deployment.environment", "string", "—", "Legacy: deployment.environment attribute (deprecated, use resource JSON)"
   "trace_id.property", "string", "trace_id", "Property name containing trace ID (32 hex characters = 128 bits)"
   "span_id.property", "string", "span_id", "Property name containing span ID (16 hex characters = 64 bits)"
   "trace_flags.property", "string", "trace_flags", "Property name containing trace flags (hex string, 0-255)"
   "attributeMap", "string (JSON object)", "—", "Custom mapping of rsyslog properties to OTLP attribute names. JSON object with rsyslog property names as keys and OTLP attribute names as values. Supported properties: ``hostname``, ``appname``, ``procid``, ``msgid``, ``facility``."
   "severity.map", "string (JSON object)", "—", "Custom mapping of syslog priorities to OTLP severity. JSON object with priority numbers (0-7) as keys and objects with ``number`` (OTLP severity number) and ``text`` (OTLP severity text) as values."
   "tls.cacert", "string", "—", "Path to CA certificate bundle file (PEM format)"
   "tls.cadir", "string", "—", "Path to directory containing CA certificates"
   "tls.cert", "string", "—", "Path to client certificate file (PEM format) for mTLS"
   "tls.key", "string", "—", "Path to client private key file (PEM format) for mTLS"
   "tls.verify_hostname", "boolean", "on", "Enable/disable hostname verification in certificate"
   "tls.verify_peer", "boolean", "on", "Enable/disable peer certificate verification"
   "proxy", "string", "—", "Proxy server URL. Must include scheme: ``http://``, ``https://``, ``socks4://``, or ``socks5://``. Example: ``http://proxy.example.com:8080``. The proxy type is automatically detected from the URL scheme. If omitted, direct connections are used."
   "proxy.user", "string", "—", "Proxy username for authentication. Required when proxy requires authentication. Used with ``proxy.password`` to enable basic or digest authentication. If ``proxy.user`` is provided without ``proxy.password``, an empty password is used."
   "proxy.password", "string", "—", "Proxy password for authentication. Required when proxy requires authentication. Used together with ``proxy.user``. The authentication method (basic or digest) is automatically negotiated with the proxy server."

Batch sizes are estimated from the body length plus per-record overhead so the
module can limit payloads without rendering JSON for each candidate message.
When a threshold is reached the batch is flushed immediately; otherwise it is
sent when either the timeout elapses or rsyslog finishes the current queue
transaction.

Environment variables from the OpenTelemetry specification are consulted when a
configuration omits explicit values. ``endpoint`` falls back to
``OTEL_EXPORTER_OTLP_LOGS_ENDPOINT`` and then ``OTEL_EXPORTER_OTLP_ENDPOINT``;
``protocol`` and ``compression`` honour the matching ``*_PROTOCOL`` and
``*_COMPRESSION`` variables; ``timeout.ms`` defaults to
``OTEL_EXPORTER_OTLP_LOGS_TIMEOUT`` or ``OTEL_EXPORTER_OTLP_TIMEOUT``; and
``headers`` consumes ``OTEL_EXPORTER_OTLP_LOGS_HEADERS`` or the generic
``OTEL_EXPORTER_OTLP_HEADERS`` when no explicit headers are configured. Action
parameters always take precedence, giving operators an easy override when
reusing existing collector deployments.

When the endpoint string includes an explicit path (for example,
``https://otel:4318/v1/logs``), the module automatically splits the final path
segment into the ``path`` parameter so that future HTTP transport code can join
the pieces without duplicating ``/v1/logs``.

Example
-------

The example below batches up to 200 records or 256 KiB every two seconds, uses
gzip compression, and adds a custom tenant header alongside an Authorization
bearer token read from the environment. Retry/backoff settings align with the
default action queue behaviour: HTTP status codes ``429`` and ``5xx`` trigger
``RS_RET_SUSPENDED`` so queued records are retried with jittered backoff, while
other non-success responses discard the message and log an error.

.. code-block:: none

   module(load="omotel")
   action(
     type="omotel"
     endpoint="https://otel-collector:4318"
     path="/v1/logs"
     protocol="http/json"
     template="RSYSLOG_SyslogProtocol23Format"
     batch.max_items="200"
     batch.max_bytes="262144"
     batch.timeout.ms="2000"
     compression="gzip"
     retry.max_retries="7"
     retry.max.ms="45000"
     headers='{ "X-OTel-Tenant": "blue" }'
     bearer_token="${env:OTEL_TOKEN}"
   )

.. note::

   When using an ``https://`` endpoint, TLS configuration is recommended for
   proper certificate verification. If the system's default CA certificates are
   sufficient, TLS parameters may be omitted. For production deployments or when
   using custom CA certificates or mTLS, configure the appropriate ``tls.*``
   parameters as shown in the :ref:`TLS Configuration Example <tls-config-example>`.
   For environments requiring proxy support (corporate networks, firewalls), see
   the :ref:`Proxy Configuration Example <proxy-config-example>`.

Trace Correlation Example
-------------------------

The following example demonstrates how to extract trace context from JSON messages
and populate OTLP trace correlation fields. The trace context is extracted from
the message using ``mmjsonparse`` and then exported via ``omotel`` with trace
correlation enabled.

.. code-block:: none

   module(load="mmjsonparse")
   module(load="omotel")

   # Parse JSON messages to extract trace properties
   action(type="mmjsonparse" mode="find-json")

   # Export with trace correlation
   action(
     type="omotel"
     endpoint="https://otel-collector:4318"
     path="/v1/logs"
     trace_id.property="trace_id"
     span_id.property="span_id"
     trace_flags.property="trace_flags"
   )

In this example, if a JSON message contains fields like ``trace_id``, ``span_id``,
or ``trace_flags``, they will be extracted and included in the OTLP export. The
property names can be customized using the ``trace_id.property``, ``span_id.property``,
and ``trace_flags.property`` parameters.

Resource Configuration Example
-------------------------------

The following example demonstrates how to configure full resource attributes using
the ``resource`` parameter with a JSON object. This enables full OpenTelemetry
resource semantic conventions compliance.

.. code-block:: none

   module(load="omotel")
   action(
     type="omotel"
     endpoint="https://otel-collector:4318"
     path="/v1/logs"
     resource='{
       "service.name": "my-application",
       "service.instance.id": "${hostname}",
       "service.version": "1.2.3",
       "deployment.environment": "production",
       "cloud.provider": "aws",
       "cloud.region": "us-east-1",
       "k8s.namespace.name": "default",
       "k8s.pod.name": "${hostname}"
     }'
   )

The resource attributes are merged with automatic attributes (``service.name``,
``telemetry.sdk.*``) that are always added by the module. The resource JSON
supports string, integer, and boolean values. Boolean values are exported as
OTLP boolean attributes using the ``boolValue`` field.

.. _tls-config-example:

TLS Configuration Example
---------------------------

The following example demonstrates how to configure TLS/mTLS for secure HTTPS
connections. This enables server certificate verification using a CA certificate
bundle and optional mutual TLS authentication with client certificates.

.. code-block:: none

   module(load="omotel")
   action(
     type="omotel"
     endpoint="https://otel-collector:4318"
     path="/v1/logs"
     tls.cacert="/etc/ssl/certs/ca-bundle.pem"
     tls.cert="/etc/rsyslog/client.pem"
     tls.key="/etc/rsyslog/client-key.pem"
     tls.verify_hostname="on"
     tls.verify_peer="on"
   )

The TLS parameters support:

- **tls.cacert**: Path to a CA certificate bundle file (PEM format) for server
  certificate verification
- **tls.cadir**: Path to a directory containing CA certificates (alternative to
  tls.cacert)
- **tls.cert**: Path to a client certificate file (PEM format) for mutual TLS
  authentication
- **tls.key**: Path to a client private key file (PEM format) for mutual TLS
  authentication
- **tls.verify_hostname**: Enable/disable hostname verification in the server
  certificate (default: ``on``)
- **tls.verify_peer**: Enable/disable peer certificate verification (default: ``on``)

File paths are validated at configuration time. Invalid or inaccessible
certificate files will cause configuration to fail with an error message
indicating the specific file and reason (e.g., "tls.cacert file '/path/to/ca.pem'
cannot be accessed: No such file or directory"). If both ``tls.cacert`` and
``tls.cadir`` are specified, both are used (libcurl behavior). Client certificate
and key must be provided together for mTLS. The default behavior verifies both
hostname and peer certificate for secure connections.

When TLS verification fails at runtime (e.g., certificate mismatch, expired
certificate, or hostname mismatch), the HTTP request fails and the batch is
retried according to the configured retry policy. TLS verification errors are
logged with details from libcurl (e.g., "SSL: certificate subject name does not
match target host name"). For test environments or self-signed certificates,
you may need to set ``tls.verify_hostname="off"`` if certificates don't have
proper Subject Alternative Name (SAN) entries matching the endpoint hostname.

Note that ``tls.verify_hostname`` requires ``tls.verify_peer`` to be enabled
(``on``) for meaningful verification. If peer verification is disabled,
hostname verification is effectively ignored.

.. _proxy-config-example:

Proxy Configuration Example
---------------------------

The following example demonstrates how to configure HTTP proxy support for
corporate networks or environments that require traffic to pass through a proxy
server. Proxy support enables omotel to work through firewalls, network gateways,
and corporate proxies.

.. code-block:: none

   module(load="omotel")
   action(
     type="omotel"
     endpoint="https://otel-collector:4318"
     path="/v1/logs"
     proxy="http://proxy.example.com:8080"
   )

For proxies that require authentication, provide both username and password:

.. code-block:: none

   module(load="omotel")
   action(
     type="omotel"
     endpoint="https://otel-collector:4318"
     path="/v1/logs"
     proxy="http://proxy.example.com:8080"
     proxy.user="myuser"
     proxy.password="mypassword"
   )

The proxy parameters support:

- **proxy**: Full proxy server URL including scheme and port. Supported schemes:
  
  - ``http://`` - HTTP proxy (most common for corporate proxies)
  - ``https://`` - HTTPS proxy (secure proxy connection)
  - ``socks4://`` - SOCKS4 proxy
  - ``socks5://`` - SOCKS5 proxy (supports authentication and UDP)

  The proxy URL format is validated at configuration time. Invalid schemes or
  malformed URLs will cause configuration to fail with an error message.

- **proxy.user**: Username for proxy authentication. When provided, the module
  automatically sends proxy authentication credentials with each request. The
  authentication method (basic or digest) is automatically negotiated with the
  proxy server based on the proxy's requirements.

- **proxy.password**: Password for proxy authentication. Must be provided
  together with ``proxy.user`` when the proxy requires authentication. If
  ``proxy.user`` is specified without ``proxy.password``, an empty password is
  used (which may work for some proxy configurations).

Proxy authentication uses HTTP Basic authentication by default. If the proxy
server requires Digest authentication, libcurl automatically upgrades to Digest
when the proxy responds with a 407 (Proxy Authentication Required) status
code. The authentication credentials are sent in the ``Proxy-Authorization``
header for each HTTP request.

When a proxy is configured, all HTTP requests to the OTLP collector endpoint
are routed through the proxy server. The proxy acts as an intermediary,
forwarding requests to the target collector and returning responses. This
enables omotel to work in environments where:

- Direct connections to the collector are blocked by firewall rules
- Network traffic must pass through a corporate proxy gateway
- Network policies require all outbound HTTP/HTTPS traffic to use a proxy
- The collector is only accessible through a proxy server

Proxy configuration works with both HTTP and HTTPS endpoints. When using an
HTTPS endpoint through an HTTP proxy, the proxy performs HTTP CONNECT tunneling
to establish the secure connection. SOCKS proxies (SOCKS4 and SOCKS5) can also
handle both HTTP and HTTPS traffic transparently.

If proxy authentication fails (invalid credentials or unsupported authentication
method), the HTTP request fails and the batch is retried according to the
configured retry policy. Authentication errors are logged with details from
libcurl (e.g., "Proxy authentication required" or "Invalid proxy credentials").

Note that proxy configuration is independent of TLS configuration. You can use
both proxy and TLS parameters together. For example, you might route HTTPS
traffic through an HTTP proxy, or use an HTTPS proxy with TLS verification
enabled for the collector endpoint.

Legacy single-attribute parameters (``resource.service_instance_id`` and
``resource.deployment.environment``) are still supported for backward compatibility
but may be deprecated in future versions.

Trace IDs must be exactly 32 hexadecimal
characters (128 bits), span IDs must be exactly 16 hexadecimal characters (64 bits),
and trace flags are parsed as hexadecimal values (0-255). Invalid values are logged
but do not cause message processing to fail.

OTLP Payload Structure
----------------------

The module generates OTLP/HTTP JSON payloads conforming to the OpenTelemetry
log data model. Each batch wraps log records in the following hierarchy:

**Resource attributes** (per-batch, automatically populated):

.. csv-table::
   :header: "Attribute", "Value"
   :widths: auto

   "``service.name``", "``rsyslog``"
   "``telemetry.sdk.name``", "``rsyslog-omotel``"
   "``telemetry.sdk.language``", "``C``"
   "``telemetry.sdk.version``", "rsyslog version string"
   "``host.name``", "hostname (only if uniform across all records in batch)"

**Scope metadata** (per-batch):

.. csv-table::
   :header: "Field", "Value"
   :widths: auto

   "``scope.name``", "``rsyslog.omotel``"
   "``scope.version``", "rsyslog version string"

**Per-record attributes** (derived from syslog metadata):

.. csv-table::
   :header: "Attribute", "Source"
   :widths: auto

   "``log.syslog.appname``", "syslog APP-NAME field (customizable via ``attributeMap``)"
   "``log.syslog.procid``", "syslog PROCID field (customizable via ``attributeMap``)"
   "``log.syslog.msgid``", "syslog MSGID field (customizable via ``attributeMap``)"
   "``log.syslog.facility``", "syslog facility code (integer, customizable via ``attributeMap``)"
   "``log.syslog.hostname``", "syslog HOSTNAME field (customizable via ``attributeMap``)"

**Per-record fields**:

- ``body.stringValue``: rendered template output
- ``timeUnixNano``: message timestamp in nanoseconds
- ``observedTimeUnixNano``: reception timestamp in nanoseconds
- ``severityNumber``: mapped OTLP severity (see table below)
- ``severityText``: severity name string
- ``traceId``: trace ID string (32 hex characters, if present in message properties)
- ``spanId``: span ID string (16 hex characters, if present in message properties)
- ``flags``: trace flags integer (0-255, if present in message properties)

All of the attributes and fields above are emitted for every record; values are
only omitted when the originating syslog message does not carry the associated
property (for example, when ``APP-NAME`` is empty). Trace correlation fields
(traceId, spanId, flags) are populated from message JSON variables when present
and valid.

Severity Mapping
^^^^^^^^^^^^^^^^

Syslog priority values are mapped to OTLP severity numbers following the
OpenTelemetry semantic conventions. The default mapping can be overridden using
the ``severity.map`` parameter.

Default severity mapping:

.. csv-table::
   :header: "Syslog Priority", "OTLP SeverityNumber", "OTLP SeverityText"
   :widths: auto

   "0 (Emergency)", "24", "EMERGENCY"
   "1 (Alert)", "23", "ALERT"
   "2 (Critical)", "22", "CRITICAL"
   "3 (Error)", "17", "ERROR"
   "4 (Warning)", "13", "WARNING"
   "5 (Notice)", "11", "NOTICE"
   "6 (Info)", "9", "INFO"
   "7 (Debug)", "5", "DEBUG"

Custom Attribute and Severity Mapping Example
----------------------------------------------

The following example demonstrates how to customize attribute names and severity
mapping to match collector expectations or custom severity schemes:

.. code-block:: none

   module(load="omotel")
   action(
     type="omotel"
     endpoint="https://otel-collector:4318"
     path="/v1/logs"
     attributeMap='{
       "hostname": "host.name",
       "appname": "service.name",
       "procid": "process.pid",
       "msgid": "message.id",
       "facility": "log.syslog.facility.code"
     }'
     severity.map='{
       "0": { "number": 24, "text": "FATAL" },
       "1": { "number": 23, "text": "ALERT" },
       "2": { "number": 22, "text": "CRITICAL" },
       "3": { "number": 17, "text": "ERROR" },
       "4": { "number": 13, "text": "WARN" },
       "5": { "number": 11, "text": "NOTICE" },
       "6": { "number": 9, "text": "INFO" },
       "7": { "number": 5, "text": "DEBUG" }
     }'
   )

The ``attributeMap`` parameter allows you to remap standard syslog properties
to different OTLP attribute names. This is useful when integrating with
collectors that expect different attribute naming conventions.

The ``severity.map`` parameter allows you to customize the mapping of syslog
priorities (0-7) to OTLP severity numbers and text. All 8 priorities must be
specified in the mapping, or defaults will be used for missing priorities.
Custom severity numbers should follow OTLP severity conventions (0-24 range
recommended).

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each
worker instance. The statistic origin is named "omotel" with the instance URL
appended (e.g., "omotel-http://127.0.0.1:4318/v1/logs"). Statistics are visible
via the :doc:`impstats <../modules/impstats>` module when enabled.

The following counters are tracked per worker instance:

- **batches.submitted** - Total number of batches submitted for transmission.
  Each batch may contain multiple log records.

- **batches.success** - Number of batches successfully sent (HTTP 2xx response).

- **batches.retried** - Number of batches that triggered retry logic due to
  retryable HTTP errors (5xx or 429 status codes).

- **batches.dropped** - Number of batches dropped due to non-retryable errors
  (HTTP 4xx status codes).

- **http.status.4xx** - Total number of HTTP 4xx responses received from the
  collector.

- **http.status.5xx** - Total number of HTTP 5xx responses received from the
  collector.

- **records.sent** - Total number of log records successfully sent to the
  collector (only incremented for successful batches).

- **http.request.latency.ms** - Cumulative request latency in milliseconds
  across all HTTP requests. This value represents the total time spent waiting
  for HTTP responses and can be used to calculate average latency by dividing
  by the number of requests.

Counters are thread-safe and use atomic operations for updates. Each worker
instance maintains its own statistics object, allowing operators to monitor
performance per action instance when multiple omotel actions are configured.
