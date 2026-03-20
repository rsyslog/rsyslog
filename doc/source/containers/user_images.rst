.. _containers-user-images:

User-Focused Images
-------------------

.. meta::
   :description: Official rsyslog container image variants for end users, including minimal, standard, collector, dockerlogs, and ETL roles.
   :keywords: rsyslog, containers, Docker, minimal, collector, dockerlogs, ETL

.. summary-start

The official rsyslog image family provides layered container variants for custom builds, general-purpose forwarding, central collection, Docker log forwarding, and Vespa-oriented ETL pipelines.

.. summary-end

.. index::
   pair: containers; user images
   pair: Docker; rsyslog images

Official images are built on Ubuntu LTS releases and install the latest
rsyslog packages from the Adiscon daily-stable PPA. Images are tagged
``rsyslog/rsyslog[-<function>]:<version>`` where ``<version>`` follows
the ``YYYY-MM`` rsyslog release.

Available variants include:

* :doc:`rsyslog/rsyslog-minimal <minimal>` – rsyslog core only.
* :doc:`rsyslog/rsyslog <standard>` – standard image with common modules
  such as ``imhttp`` and ``omhttp``.
* :doc:`rsyslog/rsyslog-collector <collector>` – adds modules for
  centralized log collection, including RELP and optional TLS reception.
* :doc:`rsyslog/rsyslog-dockerlogs <dockerlogs>` – includes ``imdocker``
  to process logs from the Docker daemon.
* :doc:`rsyslog/rsyslog-etl <etl>` – receives syslog and forwards events
  to a Vespa HTTP endpoint using ``omhttp``.
* ``rsyslog/rsyslog-debug`` – planned variant with troubleshooting tools.

.. toctree::
   :maxdepth: 1

   minimal
   standard
   collector
   dockerlogs
   etl

Images are built using the layered ``Makefile`` in
``packaging/docker/rsyslog``::

    make all
    make standard
    make VERSION=2025-06 minimal

Configuration snippets in each directory and environment variables
allow enabling or disabling functionality at runtime. Consult the
`packaging/docker README <https://github.com/rsyslog/rsyslog/tree/main/packaging/docker>`_
for the latest details and feedback instructions.

For a complete centralized logging stack with Loki, Grafana, Prometheus, and
Traefik, see :doc:`../deployments/rosi_collector/index`. ROSI Collector uses
the ``rsyslog/rsyslog-collector`` image as one component inside a larger Docker
Compose deployment.
