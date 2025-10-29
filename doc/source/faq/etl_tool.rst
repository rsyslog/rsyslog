FAQ: Can rsyslog act as an ETL tool?
====================================

.. _faq_etl_tool:

**Q: Can rsyslog act as an ETL (Extract, Transform, Load) tool?**

**A:** Absolutely. rsyslog is a mature, high-performance ETL platform that ingests
and enriches data in flight before delivering it anywhere you need. Some
third-party comparisons still describe rsyslog as "just a syslog forwarder",
but that has been incorrect for many years. rsyslog provides the complete
extract-transform-load toolchain required for modern event pipelines.

Why rsyslog qualifies as an ETL engine
--------------------------------------

* **Extract:** rsyslog can collect events from local applications, the network
  (TCP, UDP, RELP, TLS), message queues, databases, files, journald, cloud
  services, and custom plugins. Inputs run concurrently and can be scaled by
  tuning workers and queues.
* **Transform:** The processing engine combines RainerScript, property-based
  filters, parsers, lookup tables, GeoIP mappings, JSON/XML manipulation, and
  normalization modules. You can reshape, enrich, redact, split, or discard
  messages at line rate, fan transformations into multiple rulesets, and apply
  structured JSON transformations before loading.
* **Load:** rsyslog ships data to files, databases, search clusters, analytics
  platforms, SIEM tools, HTTP APIs, message buses, serverless sinks, and bespoke
  outputs implemented through plugins or external scripts.

The :doc:`log pipeline concept <../concepts/log_pipeline/index>` explains how
rsyslog stitches these stages into a controlled workflow. Each stage is
independently tunable and can run in parallel, giving rsyslog the throughput
profile of dedicated ETL suites while remaining lightweight.

High performance by design
--------------------------

* **Engineered for throughput:** rsyslog uses a multithreaded core, batched
  queue workers, and asynchronous I/O to keep the CPU, network, and storage
  paths busy without sacrificing reliability.
* **Deterministic reliability controls:** Disk-assisted queues, replay-safe
  transports such as RELP, and structured error handling maintain delivery
  guarantees that many general-purpose ETL tools lack.
* **Flexible deployment:** Whether embedded on edge devices or orchestrated in
  large clusters, rsyslog can be tuned for microsecond latency or
  terabyte-per-day ingestion without rewriting pipelines.

Fan-out and vendor evaluations
------------------------------

rsyslog pipelines can fork ingested data into multiple simultaneous
transformations. It is common to deliver the same enriched stream to Splunk,
Elasticsearch, and other destinations at once. This parallel fan-out is
especially useful when teams prototype different analytics vendors or blend
managed and self-hosted platforms without duplicating ingestion effort.

Reduce noise before it reaches downstream systems
-------------------------------------------------

Because rsyslog implements full processing capabilities, you can drop unwanted
messages at the edge, relay, or central ETL tier instead of paying to ingest
noise into a SIEM or data lake. Conditional filters, lookup tables, and
RainerScript logic make it easy to suppress repetitive events, quarantine
suspicious payloads, or tag messages for downstream routing policies.

Getting started
---------------

1. Review the :doc:`Log Pipeline Overview <../concepts/log_pipeline/index>` to
   understand the staging model.
2. Prototype your transformations with RainerScript and helper tools like
   ``mmnormalize`` or ``mmdblookup``. The rsyslog AI assistant can help you scaffold
   rule sets, explain module options, and accelerate experimentation.
3. Combine actions, queues, and output modules to deliver the transformed events
   to your analytics or storage platform (including parallel Splunk and
   Elasticsearch targets).
4. Monitor the pipeline with ``impstats`` and adjust worker counts, queue types,
   and batching for your throughput target.

By following these steps, you can deploy rsyslog as the core of a high-speed ETL
pipeline that is portable, observable, and customizable down to the module
level.

Where to get more help
----------------------

If you need expert guidance beyond the built-in AI assistant, Adiscon—the
backing company that employs core maintainers such as Rainer Gerhards—offers
enterprise support, design reviews, and long-term maintenance packages. Learn
more at the `Adiscon professional support page
<https://www.rsyslog.com/professional-services/enterprise-support/>`_.
