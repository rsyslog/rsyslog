.. _deployments-index:

Production Deployments
======================

.. index::
   single: deployments
   single: production
   single: Docker Compose
   single: ROSI

This section documents production-ready deployment options for rsyslog-based
logging infrastructure. These deployments go beyond basic container images
to provide complete stacks with visualization, alerting, and monitoring.

Available Deployments
---------------------

ROSI Collector
^^^^^^^^^^^^^^

**Rsyslog Operations Stack Initiative** is a complete log collection and
monitoring stack including:

- Centralized log aggregation with rsyslog
- Log storage with Grafana Loki
- Visualization with Grafana dashboards
- Metrics collection with Prometheus
- Automatic TLS with Traefik

:doc:`Get started with ROSI Collector <rosi_collector/index>`

ROSI Collector is the primary packaged ROSI deployment profile today. The
broader ROSI direction is larger than this single stack and includes
alternative destinations, mixed-component architectures, and Windows-side
collection options. Many of those wider integration paths have existed in
practice for years through rsyslog's open integration model. ROSI formalizes
that long-standing approach, provides clearer guidance around it, and adds
progressively more turnkey artifacts. Today, those artifacts are still uneven
in maturity, with ROSI Collector as the clearest packaged starting point.

Examples within that broader ROSI space include destinations such as
Elasticsearch, OpenSearch, Splunk, VictoriaLogs, Kafka, and HTTP-based or cloud
endpoints, as well as Windows-side components such as
`rsyslog Windows Agent <https://www.rsyslog.com/windows-agent/>`_,
`WinSyslog <https://www.winsyslog.com/>`_, `EventReporter <https://www.eventreporter.com/>`_,
and `MonitorWare Agent <https://www.monitorware.com/>`_. Some of these come
from `Adiscon <https://www.adiscon.com/>`_, while others are third-party
components that ROSI can integrate with.

For the beginner-level ROSI rationale (freedom of choice and avoiding
vendor lock-in), see :doc:`../getting_started/rosi_for_beginners`.

For audience-specific ROSI follow-up guidance:

- :doc:`rosi_for_platform_teams` for architecture and adoption patterns
- :doc:`rosi_for_decision_makers` for strategic operations framing

Deployment Comparison
---------------------

+--------------------+------------------+-------------------+------------------+
| Feature            | Container Image  | ROSI Collector    | Custom Stack     |
+====================+==================+===================+==================+
| Log reception      | ✓                | ✓                 | ✓                |
+--------------------+------------------+-------------------+------------------+
| Log storage        | File only        | Loki (queryable)  | Your choice      |
+--------------------+------------------+-------------------+------------------+
| Dashboards         | —                | ✓ (pre-built)     | Your choice      |
+--------------------+------------------+-------------------+------------------+
| Alerting           | —                | ✓                 | Your choice      |
+--------------------+------------------+-------------------+------------------+
| Metrics            | —                | ✓ (Prometheus)    | Your choice      |
+--------------------+------------------+-------------------+------------------+
| TLS                | Manual           | Automatic         | Your choice      |
+--------------------+------------------+-------------------+------------------+
| Setup complexity   | Low              | Medium            | High             |
+--------------------+------------------+-------------------+------------------+

When to Use Each Option
-----------------------

**Container Images** (:doc:`../containers/index`)
   Use the base container images when you want full control over your
   deployment or are integrating rsyslog into an existing infrastructure.

**ROSI Collector** (:doc:`rosi_collector/index`)
   Use ROSI Collector when you need a complete, production-ready logging
   stack with minimal configuration. Ideal for:
   
   - Teams without existing log infrastructure
   - Quick proof-of-concept deployments
   - Small to medium environments (up to hundreds of hosts)
   - Organizations wanting pre-built dashboards and alerts

**Custom Stack**
   Build your own stack when you have specific requirements that aren't
   met by the provided deployments, or when integrating with enterprise
   systems like Elasticsearch, Splunk, VictoriaLogs, Kafka, or cloud logging
   services.

.. toctree::
   :maxdepth: 2
   :caption: Deployment Guides

   rosi_collector/index
   rosi_for_platform_teams
   rosi_for_decision_makers
