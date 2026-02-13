.. _tutorial-impstats-push-victoriametrics:

.. meta::
   :description: Step-by-step tutorial for pushing impstats counters to VictoriaMetrics using Prometheus Remote Write.
   :keywords: rsyslog, impstats, VictoriaMetrics, Prometheus Remote Write, tutorial, push mode

.. summary-start

Learn how to configure rsyslog impstats push mode, send counters to
VictoriaMetrics, verify data end-to-end, and troubleshoot common failures.

.. summary-end

Tutorial: impstats Push to VictoriaMetrics
==========================================

Goal
----

Configure rsyslog to push impstats counters directly to VictoriaMetrics
with Prometheus Remote Write, then verify that metrics are queryable.


What you will set up
--------------------

``impstats`` (rsyslog) -> ``/api/v1/write`` (VictoriaMetrics)

.. mermaid::

   flowchart LR
     RS["rsyslog impstats interval"] --> RW["Remote Write payload (protobuf + snappy)"]
     RW --> VM["VictoriaMetrics /api/v1/write"]
     VM --> Q["Query /api/v1/query"]


Prerequisites
-------------

1. rsyslog with impstats push support enabled.
2. A reachable VictoriaMetrics endpoint.
3. Permission to restart rsyslog.

You can confirm configuration parsing support with:

.. code-block:: bash

   rsyslogd -N1

If your config contains ``push.url`` and parsing fails with unknown parameter
errors, your build likely does not include impstats push support.


Step 1: Start VictoriaMetrics (local test)
------------------------------------------

.. code-block:: bash

   docker run -d --name vm -p 8428:8428 victoriametrics/victoria-metrics


Step 2: Configure impstats push mode
------------------------------------

Create a test config file:

.. code-block:: rsyslog

   module(load="impstats"
          interval="10"
          format="json"
          log.file="/var/log/rsyslog-stats.log"
          push.url="http://127.0.0.1:8428/api/v1/write"
          push.timeout.ms="2000"
          push.labels=["env=dev", "pipeline=impstats-push"]
          push.label.origin="on"
          push.label.name="on")

Use your normal rsyslog configuration layout if preferred; the important part
is the ``module(load="impstats" ...)`` block with ``push.url``.


Step 3: Reload or restart rsyslog
---------------------------------

.. code-block:: bash

   sudo systemctl restart rsyslog


Step 4: Verify metrics in VictoriaMetrics
-----------------------------------------

Query example metrics:

.. code-block:: bash

   curl -s http://127.0.0.1:8428/api/v1/query \
     -d 'query=impstats_resource_usage_utime_total'

   curl -s http://127.0.0.1:8428/api/v1/query \
     -d 'query=impstats_resource_usage_openfiles_total'

Expected:

1. ``status`` is ``success`` in API response.
2. Result contains one or more time series with recent timestamps.
3. Labels include your static labels (for example ``env=dev``).


How names and labels appear
---------------------------

Metric naming:

- ``<origin>_<name>_<counter>_total``
- If ``name`` is empty, it is omitted.
- Invalid Prometheus characters are sanitized to ``_``.

Label behavior:

- Static labels from ``push.labels`` are always included.
- Optional dynamic labels can add ``instance``, ``job``, ``origin``, ``name``.
- If a key already exists in ``push.labels``, it is not overridden by dynamic
  label injection.


Troubleshooting quick checks
----------------------------

.. mermaid::

   flowchart TD
     A["No metrics in query result"] --> B{"Any impstats push errors in rsyslog logs?"}
     B -->|No| C["Check endpoint URL and local reachability"]
     B -->|Yes| D{"Error type?"}
     D -->|TLS| E["Verify https URL, CA/cert/key settings, cert+key pairing"]
     D -->|HTTP 4xx| F["Fix request/config mismatch at endpoint"]
     D -->|HTTP 5xx or timeout| G["Backend issue; impstats retries next interval"]
     C --> H["Re-check with /api/v1/query"]
     E --> H
     F --> H
     G --> H
     H --> I{"Metrics visible?"}
     I -->|No| J["Enable debug whitelist for impstats files and inspect logs"]
     I -->|Yes| K["Done"]

Push endpoint down:

1. Check rsyslog logs for push HTTP failure messages.
2. Validate endpoint reachability from rsyslog host.

TLS mismatch:

1. ``push.tls.*`` applies only to ``https://`` URLs.
2. ``push.tls.certfile`` and ``push.tls.keyfile`` must be configured together.

4xx versus 5xx behavior:

1. ``4xx`` generally indicates request/configuration issues to fix.
2. ``5xx`` is treated as retryable; impstats retries on next interval.


Production tuning notes
-----------------------

1. Keep ``interval`` aligned with your observability needs and endpoint capacity.
2. Use ``push.batch.maxSeries`` to cap per-request series count.
3. Use ``push.batch.maxBytes`` for approximate payload size targeting.
4. Keep label cardinality controlled; avoid high-cardinality custom labels.
5. Remember push is synchronous and no cross-interval push buffering is used.


See also
--------

- :doc:`../configuration/modules/impstats`
- :doc:`../faq/impstats-push-mode`
- :doc:`../reference/parameters/impstats-push-url`
- :doc:`../reference/parameters/impstats-push-batch-maxbytes`
- :doc:`../reference/parameters/impstats-push-batch-maxseries`
