.. _rosi-collector-architecture:

Architecture
============

.. index::
   pair: ROSI Collector; architecture
   single: Loki
   single: Prometheus
   single: Traefik

ROSI Collector combines several components into a cohesive logging and
monitoring stack. This page describes each component, how they interact,
and the data flows through the system.

.. figure:: rosi-architecture.svg
   :alt: ROSI Collector Architecture Diagram
   :align: center
   :width: 100%
   
   ROSI Collector architecture showing data flows between components

Component Overview
------------------

rsyslog (Log Receiver)
^^^^^^^^^^^^^^^^^^^^^^

The rsyslog container receives logs from client hosts over TCP port 10514.
It processes incoming messages and forwards them directly to Loki using
the omhttp output module. rsyslog provides:

- High-performance log reception (handles thousands of messages/second)
- Message parsing and normalization
- Queue-based reliability (messages survive brief outages)
- Direct Loki integration via omhttp module
- Optional JSON file output for backup/debugging

Grafana Loki (Log Storage)
^^^^^^^^^^^^^^^^^^^^^^^^^^

Loki stores logs in a compressed, indexed format optimized for LogQL
queries. Unlike traditional log management systems, Loki indexes only
labels (metadata) rather than full-text, making it highly efficient:

- 30-day default retention
- Label-based indexing for fast queries
- Efficient compression reduces storage needs
- LogQL query language (similar to PromQL)

Prometheus (Metrics)
^^^^^^^^^^^^^^^^^^^^

Prometheus scrapes metrics from node_exporter running on client hosts.
It stores time-series data and provides alerting capabilities:

- Automatic service discovery from targets file
- 15-day metrics retention
- PromQL for metric queries and alerts
- Recording rules for dashboard performance

Grafana (Visualization)
^^^^^^^^^^^^^^^^^^^^^^^

Grafana provides the web interface for exploring logs and viewing
dashboards. It connects to both Loki (logs) and Prometheus (metrics):

- Pre-provisioned dashboards
- Explore interface for ad-hoc queries
- Alerting integration
- User authentication

Traefik (Reverse Proxy)
^^^^^^^^^^^^^^^^^^^^^^^

Traefik handles external access to the stack, providing:

- Automatic TLS certificates via Let's Encrypt
- Basic authentication for Prometheus/Alertmanager
- Request routing to internal services
- HTTP to HTTPS redirect

Downloads (File Server)
^^^^^^^^^^^^^^^^^^^^^^^

A lightweight nginx container serves client setup files:

- Installation scripts (rsyslog client, node_exporter)
- Configuration templates
- CA certificates (when TLS is enabled)
- Client certificate packages (for mTLS)

Files are accessible at ``https://grafana.TRAEFIK_DOMAIN/downloads/``.

Data Flow
---------

Log Data Flow
^^^^^^^^^^^^^

1. **Client hosts** run rsyslog configured to forward logs to the collector
2. **Collector rsyslog** receives logs on TCP 10514
3. **rsyslog omhttp** sends logs directly to Loki with labels
4. **Loki** stores labeled log entries in compressed format
5. **Grafana** queries Loki to display logs in dashboards and Explore

Metrics Data Flow
^^^^^^^^^^^^^^^^^

1. **Client hosts** run node_exporter exposing system metrics on port 9100
2. **Prometheus** scrapes metrics from targets listed in ``nodes.yml``
3. **Grafana** queries Prometheus to display metrics in dashboards

Network Ports
-------------

+-----------+--------+-----------+------------------------------------------+
| Service   | Port   | Protocol  | Description                              |
+===========+========+===========+==========================================+
| Traefik   | 80     | TCP       | HTTP (redirects to HTTPS)                |
+-----------+--------+-----------+------------------------------------------+
| Traefik   | 443    | TCP       | HTTPS - external access                  |
+-----------+--------+-----------+------------------------------------------+
| rsyslog   | 514    | UDP       | Syslog reception (UDP)                   |
+-----------+--------+-----------+------------------------------------------+
| rsyslog   | 10514  | TCP       | Log reception from clients (TCP)         |
+-----------+--------+-----------+------------------------------------------+
| rsyslog   | 6514   | TCP       | TLS-encrypted syslog (optional profile)  |
+-----------+--------+-----------+------------------------------------------+
| Grafana   | 3000   | TCP       | Web UI (internal, proxied by Traefik)    |
+-----------+--------+-----------+------------------------------------------+
| Loki      | 3100   | TCP       | Log API (internal)                       |
+-----------+--------+-----------+------------------------------------------+
| Prometheus| 9090   | TCP       | Metrics API (internal, proxied)          |
+-----------+--------+-----------+------------------------------------------+

Container Services
------------------

The Docker Compose stack defines these services:

.. code-block:: yaml

   services:
     traefik:      # Reverse proxy with TLS
     rsyslog:      # Log receiver with omhttp output (TCP/UDP)
     rsyslog-tls:  # TLS-encrypted log receiver (profile: tls)
     loki:         # Log storage
     prometheus:   # Metrics collection
     grafana:      # Visualization
     downloads:    # Client setup file server

The ``rsyslog-tls`` service starts automatically when ``SYSLOG_TLS_ENABLED=true``
in your ``.env`` file:

.. code-block:: bash

   docker compose up -d

All containers communicate on an internal Docker network. Only Traefik,
rsyslog (10514/514), and rsyslog-tls (6514) are exposed to external traffic.

Storage Volumes
---------------

The stack uses Docker volumes for persistent data:

+--------------------+---------------------------+---------------------------+
| Volume             | Mount Point               | Purpose                   |
+====================+===========================+===========================+
| loki-data          | /loki                     | Log storage               |
+--------------------+---------------------------+---------------------------+
| prometheus-data    | /prometheus               | Metrics storage           |
+--------------------+---------------------------+---------------------------+
| grafana-data       | /var/lib/grafana          | Dashboards, preferences   |
+--------------------+---------------------------+---------------------------+
| rsyslog-logs       | /var/log/remote           | Received log files        |
+--------------------+---------------------------+---------------------------+
| traefik-certs      | /letsencrypt              | TLS certificates          |
+--------------------+---------------------------+---------------------------+

Resource Requirements
---------------------

Minimum requirements for a small deployment (10-50 clients):

- **CPU**: 2 cores
- **RAM**: 4 GB (Loki is the primary consumer)
- **Disk**: 50 GB (depends on log volume and retention)

For larger deployments, scale based on:

- **Log volume**: ~1 GB storage per 10 million log lines
- **Retention period**: Multiply daily volume by retention days
- **Query load**: Additional RAM improves query performance

Scaling Considerations
----------------------

ROSI Collector is designed for single-server deployments. For larger
environments, consider:

- **Horizontal scaling**: Deploy multiple collectors with load balancing
- **Loki clustering**: Run Loki in distributed mode
- **External storage**: Use S3-compatible storage for Loki chunks
- **Prometheus federation**: Aggregate metrics from multiple Prometheus
