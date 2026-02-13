.. _faq-impstats-push-mode:

.. meta::
   :description: FAQ and troubleshooting guide for impstats push mode and Prometheus Remote Write backends such as VictoriaMetrics.
   :keywords: rsyslog, impstats, push mode, Remote Write, VictoriaMetrics, Prometheus, troubleshooting

.. summary-start

Common setup and troubleshooting answers for impstats push mode
(Prometheus Remote Write / VictoriaMetrics).

.. summary-end

FAQ: impstats Push Mode (Remote Write)
======================================

Q: Do I need ``format="prometheus"`` to use push mode?
------------------------------------------------------

No. Push mode is independent from local emission format. Set ``push.url`` to
enable push, and keep local ``format``/``log.file``/``log.syslog`` according
to your local processing needs.


Q: What exactly enables push mode?
----------------------------------

Push is enabled when ``push.url`` is configured.


Q: How do I verify my build supports push mode?
------------------------------------------------

Push mode is compiled only when rsyslog is built with
``--enable-impstats-push`` (and required libraries). In project-provided
packages, push support is included.


Q: What happens when the remote endpoint is down?
-------------------------------------------------

impstats logs the push error and retries on the next impstats interval.
It does not buffer failed push payloads across intervals.


Q: Is push asynchronous?
------------------------

No. Push is synchronous in the impstats worker thread. ``push.timeout.ms``
controls how long an HTTP request may block before timing out.


Q: How are HTTP error responses handled?
----------------------------------------

Non-2xx responses fail the push for that interval. ``5xx`` responses are treated
as retryable (next interval retry). ``4xx`` responses are treated as permanent
configuration/request errors and should be fixed at configuration or endpoint
level.


Q: How are metric names generated?
----------------------------------

Metric names are constructed as ``<origin>_<name>_<counter>_total``.
If ``name`` is empty, it is omitted. Non-Prometheus-safe characters are
sanitized (for example ``-`` and ``.`` become ``_``).


Q: Which labels are attached by default?
----------------------------------------

Static labels from ``push.labels`` are always included.

By default:

- ``push.label.instance`` is ``on``
- ``push.label.job`` is ``rsyslog``
- ``push.label.origin`` is ``off``
- ``push.label.name`` is ``off``

If a label key is already present in ``push.labels``, impstats does not
override it with dynamic labels.


Q: How do batch limits behave?
------------------------------

``push.batch.maxSeries`` limits the number of time series per request.

``push.batch.maxBytes`` is an approximate size target; impstats estimates
series-per-batch from encoded payload size and splits accordingly. It is
best-effort, not a strict byte cap.


Q: Do TLS parameters work with ``http://`` URLs?
------------------------------------------------

No. ``push.tls.*`` options only apply to ``https://`` endpoints. If TLS
options are set with a non-HTTPS URL, impstats warns that those TLS options
are ignored.


Q: Do ``push.tls.certfile`` and ``push.tls.keyfile`` need to be set together?
------------------------------------------------------------------------------

Yes. They are a pair for mTLS client authentication. If only one is set,
configuration validation fails and push mode is disabled.


Q: Are there authentication parameters for HTTP Basic or Bearer token?
----------------------------------------------------------------------

Not currently. Push mode does not currently expose built-in auth parameters
for headers or credentials.


Q: Does ``resetCounters`` affect pushed values?
-----------------------------------------------

Push is executed before local formatted stats emission for each interval.
That means pushed values are collected from current counters first, and then
local emission/reset behavior is applied for the configured output path.


Q: How should I verify push mode quickly?
-----------------------------------------

1. Configure impstats with ``push.url`` and a short ``interval``.
2. Set ``global(debug.whitelist="on" debug.files=["impstats.c","impstats_push.c"])``
   in test environments if you need detailed diagnostics.
3. Check rsyslog logs for push success/failure messages and endpoint HTTP
   status codes.
4. Query your Remote Write backend for expected metric names and labels.


See also
--------

- :doc:`../configuration/modules/impstats`
- :doc:`../reference/parameters/impstats-push-url`
- :doc:`../reference/parameters/impstats-push-labels`
- :doc:`../reference/parameters/impstats-push-timeout-ms`
