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
   systems like Elasticsearch, Splunk, or cloud logging services.

.. toctree::
   :maxdepth: 2
   :caption: Deployment Guides

   rosi_collector/index
