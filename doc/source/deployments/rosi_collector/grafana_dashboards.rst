.. _rosi-collector-grafana-dashboards:

Grafana Dashboards
==================

.. index::
   pair: ROSI Collector; dashboards
   single: Grafana
   single: Syslog Explorer
   single: Syslog Health
   single: impstats
   single: LogQL

ROSI Collector includes pre-built Grafana dashboards for exploring logs
and monitoring system health. This guide explains each dashboard and how
to use them effectively.

Dashboard Overview
------------------

ROSI Collector provisions five dashboards:

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Dashboard
     - Purpose
   * - Syslog Explorer
     - Search and browse logs from all hosts
   * - Syslog Analysis
     - Distribution analysis (severity, hosts, etc.)
   * - Syslog Health
     - rsyslog impstats (queues, actions, CPU)
   * - Host Metrics Overview
     - System metrics (CPU, memory, disk) per host
   * - Alerting Overview
     - Active alerts and notification status

Syslog Explorer
---------------

.. figure:: /_static/dashboard-explorer.png
   :alt: Syslog Explorer Dashboard
   :align: center
   
   Syslog Explorer dashboard for browsing and searching logs

The Syslog Explorer is your primary interface for searching logs. It
provides:

- **Time range selector** - Choose the time window for your search
- **Host filter** - Limit results to specific hosts
- **Severity filter** - Filter by syslog severity (err, warning, info, etc.)
- **Facility filter** - Filter by syslog facility (auth, daemon, etc.)
- **Log table** - Browse log entries with timestamp, host, and message

**Common searches**:

- All errors: Set severity to ``err``
- Authentication events: Set facility to ``auth``
- Specific host: Select from host dropdown

**Using LogQL for advanced queries**:

Click "Explore" in the left sidebar and use LogQL queries:

.. code-block:: text

   # Find SSH failures
   {job="syslog"} |= "Failed password"
   
   # Errors from specific host
   {host="webserver-01"} | json | severity = "err"
   
   # Count errors by host
   sum by (host) (count_over_time({job="syslog"} | json | severity = "err" [5m]))

Syslog Analysis
---------------

.. figure:: /_static/dashboard-deepdive.png
   :alt: Syslog Analysis Dashboard
   :align: center
   
   Syslog Analysis dashboard for log distribution

This dashboard provides analytical views of log data:

- **Log volume graph** - Messages over time
- **Severity breakdown** - Pie chart of log severities
- **Top hosts** - Hosts generating the most logs
- **Error trends** - Error count over time

Use this dashboard to:

- Identify hosts with unusual log volumes
- Track error rates over time
- Find the most common error messages

Syslog Health (impstats)
------------------------

The **Syslog Health** dashboard shows rsyslog internal metrics when client
hosts run the **impstats sidecar** (installed by default with the client
setup script). Add impstats targets on the collector:

.. code-block:: bash

   prometheus-target --job impstats add CLIENT_IP:9898 host=HOSTNAME role=rsyslog network=internal
   # Or add both node_exporter and impstats in one step:
   prometheus-target add-client CLIENT_IP host=HOSTNAME role=rsyslog network=internal

The dashboard is grouped into: **Overview** (exporter count, failures, queues),
**Queues** (size, utilization, drops), **Input** (ingest rate), **Actions**
(processed/failed/suspended), and **Output & resource usage** (egress bytes
and CPU). Use the Host dropdown to filter by client.

Host Metrics Overview
---------------------

.. figure:: /_static/dashboard-node.png
   :alt: Host Metrics Overview Dashboard
   :align: center
   
   Host Metrics Overview showing system metrics

The Host Metrics Overview shows system metrics from node_exporter:

- **CPU usage** - Per-core and total utilization
- **Memory** - Used, available, and cached
- **Disk I/O** - Read/write throughput
- **Network** - Bytes in/out per interface
- **Disk space** - Usage by filesystem

Select a host from the dropdown to view its metrics. Time range applies
to all panels. (When clients run the impstats sidecar, see **Syslog Health**
for rsyslog-specific metrics.)

**Key metrics to watch**:

- CPU usage sustained above 80%
- Memory usage approaching 100%
- Disk usage above 80%
- Network errors or drops

Alerting Overview
-----------------

.. figure:: /_static/dashboard-alerts.png
   :alt: Alerting Overview Dashboard
   :align: center
   
   Alerting Overview dashboard for monitoring alert status

View active alerts and notification status:

- **Firing alerts** - Currently active alerts
- **Alert history** - Recent alert state changes
- **Silence status** - Active silences

Creating Custom Dashboards
--------------------------

You can create your own dashboards in Grafana:

1. Click the **+** icon in the left sidebar
2. Select **New Dashboard**
3. Add panels using Loki (logs) or Prometheus (metrics) as data source

**Loki query examples**:

.. code-block:: text

   # Count logs by host
   sum by (host) (count_over_time({job="syslog"}[5m]))
   
   # Specific application logs
   {job="syslog", host="appserver"} |= "myapp"
   
   # Parse and filter
   {job="syslog"} | pattern "<_> <_> <_> <facility>.<severity> <message>" | severity = "err"

**Prometheus query examples**:

.. code-block:: text

   # CPU usage percentage
   100 - (avg by (instance) (irate(node_cpu_seconds_total{mode="idle"}[5m])) * 100)
   
   # Memory usage
   (node_memory_MemTotal_bytes - node_memory_MemAvailable_bytes) / node_memory_MemTotal_bytes * 100
   
   # Disk usage
   100 - ((node_filesystem_avail_bytes / node_filesystem_size_bytes) * 100)

Exporting Dashboards
--------------------

To backup or share dashboards:

1. Open the dashboard
2. Click the gear icon (Dashboard settings)
3. Select **JSON Model**
4. Copy the JSON and save it

Or use the API:

.. code-block:: bash

   curl -s http://localhost:3000/api/dashboards/db/syslog-explorer \
     -H "Authorization: Bearer YOUR_API_KEY" | jq .

Dashboard Best Practices
------------------------

**For log exploration**:

- Start with a narrow time range
- Use filters to reduce result set
- Export to CSV for offline analysis

**For monitoring**:

- Set appropriate alert thresholds
- Use template variables for host selection
- Create summary panels for quick status

**For performance**:

- Avoid queries spanning more than 24 hours on busy systems
- Use labels to filter before pattern matching
- Limit log line display count

See Also
--------

- `Grafana Loki documentation <https://grafana.com/docs/loki/latest/>`_
- `LogQL query language <https://grafana.com/docs/loki/latest/logql/>`_
- `PromQL basics <https://prometheus.io/docs/prometheus/latest/querying/basics/>`_
