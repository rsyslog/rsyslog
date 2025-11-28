Next Steps
==========

You have installed rsyslog, configured local logging, and optionally
set up log forwarding. Here are some recommended next steps.

Production Deployments
----------------------

For a complete log collection stack with dashboards and alerting, see
ROSI Collector (RSyslog Open System for Information):

:doc:`../deployments/rosi_collector/index`

ROSI Collector provides:

- Centralized log aggregation from multiple hosts
- Pre-built Grafana dashboards for log exploration
- Prometheus metrics collection
- Automatic TLS with Traefik

Explore Modules
---------------

rsyslog's modular design allows integrations such as:

- `omelasticsearch` for Elasticsearch and OpenSearch
- `omkafka` for Kafka
- `imfile` for monitoring text log files

Filtering and Templates
-----------------------

Learn how to filter messages and format log output using templates.

See: :doc:`../configuration/index`

Debugging and Troubleshooting
-----------------------------

Use `rsyslogd -N1` to check configuration syntax and discover issues
with included snippets.

Advanced Tutorials
------------------

For more examples and step-by-step guides, visit:

:doc:`../tutorials/index`

Community and AI Assistant
--------------------------

- Join the `GitHub Discussions <https://github.com/rsyslog/rsyslog/discussions>`_
  to ask questions and share knowledge.
- Try the `AI rsyslog assistant <https://rsyslog.ai>`_ for quick
  configuration help.
