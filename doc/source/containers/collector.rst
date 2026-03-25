.. _containers-user-collector:
.. _container.image.rsyslog-collector:

rsyslog/rsyslog-collector
=========================

.. meta::
   :description: Reference for the rsyslog collector container image, including enabled inputs, file outputs, RELP, and optional TLS settings.
   :keywords: rsyslog, collector, Docker, RELP, TLS, syslog

.. summary-start

The ``rsyslog/rsyslog-collector`` image is the reusable central-receiver role in the rsyslog container family. By default it listens on UDP and TCP syslog, can optionally enable RELP and TLS, and writes received messages to local files unless you mount additional rules.

.. summary-end

.. index::
   pair: containers; rsyslog/rsyslog-collector
   pair: Docker images; rsyslog/rsyslog-collector

Overview
--------

The ``rsyslog-collector`` container image extends the standard
:doc:`standard` base with modules for **centralised log aggregation**.
It is preconfigured to receive logs via UDP, TCP, and optionally RELP or TLS.
The packaged configuration writes received messages to local files. Additional
rules can be mounted into ``/etc/rsyslog.d/`` when you want to forward to
other backends or reshape the pipeline.

This image is the recommended starting point for building a log
collector or relay service.

The packaged collector still runs as root by default. Its shipped
configuration binds privileged listener ports such as ``514`` and
``6514`` and writes files under ``/var/log``.

.. note::

   - **UDP (514/udp)** and **TCP (514/tcp)** are enabled by default.  
   - **RELP (2514/tcp)** is available but **disabled by default**.  
   - **TLS syslog (6514/tcp)** is available but **disabled by default**.
   - External deployments usually map RELP to **20514/tcp** to avoid conflicts
     with the standard syslog port.

Environment Variables
---------------------

Runtime behaviour can be tuned with the following variables:

.. _containers-user-collector-enable_udp:
.. envvar:: ENABLE_UDP

   Enable UDP syslog reception. Default ``on``.

.. _containers-user-collector-enable_tcp:
.. envvar:: ENABLE_TCP

   Enable TCP syslog reception. Default ``on``.

.. _containers-user-collector-enable_relp:
.. envvar:: ENABLE_RELP

   Enable RELP syslog reception (internal port ``2514/tcp``). Default ``off``.

.. _containers-user-collector-enable_tls:
.. envvar:: ENABLE_TLS

   Enable TLS-encrypted syslog reception on ``6514/tcp``. Default ``off``.

.. _containers-user-collector-tls_ca_file:
.. envvar:: TLS_CA_FILE

   CA certificate path for TLS listener validation. Empty by default.

.. _containers-user-collector-tls_cert_file:
.. envvar:: TLS_CERT_FILE

   Server certificate path for the TLS listener. Empty by default.

.. _containers-user-collector-tls_key_file:
.. envvar:: TLS_KEY_FILE

   Server private key path for the TLS listener. Empty by default.

.. _containers-user-collector-tls_auth_mode:
.. envvar:: TLS_AUTH_MODE

   Netstream authentication mode for the TLS listener. Default ``anon``.

.. _containers-user-collector-write_all_file:
.. envvar:: WRITE_ALL_FILE

   Write all messages to ``/var/log/all.log``. Default ``on``.

.. _containers-user-collector-write_json_file:
.. envvar:: WRITE_JSON_FILE

   Write JSON formatted messages to ``/var/log/all-json.log``. Default ``on``.

.. _containers-user-collector-rsyslog_hostname:
.. envvar:: RSYSLOG_HOSTNAME
   :noindex:

   Hostname used inside rsyslog. Defaults to the value of ``/etc/hostname`` when unset.

.. _containers-user-collector-permit_unclean_start:
.. envvar:: PERMIT_UNCLEAN_START
   :noindex:

   Skip configuration validation when set. By default ``rsyslogd -N1`` validates the configuration.

.. _containers-user-collector-rsyslog_role:
.. envvar:: RSYSLOG_ROLE
   :noindex:

   Role name consumed by the entrypoint. Defaults to ``collector``.

Port Mapping Reference
----------------------

+-----------+----------------+------------------+-----------------+
| Protocol  | Container Port | Example External | Controlled by   |
+===========+================+==================+=================+
| UDP Syslog | 514/udp       | 514/udp          | ``ENABLE_UDP``  |
+-----------+----------------+------------------+-----------------+
| TCP Syslog | 514/tcp       | 514/tcp          | ``ENABLE_TCP``  |
+-----------+----------------+------------------+-----------------+
| RELP       | 2514/tcp      | 20514/tcp        | ``ENABLE_RELP`` |
+-----------+----------------+------------------+-----------------+
| TLS Syslog | 6514/tcp      | 6514/tcp         | ``ENABLE_TLS``  |
+-----------+----------------+------------------+-----------------+

Example Deployment (docker-compose)
-----------------------------------

A minimal configuration using `docker compose`:

.. code-block:: yaml

   version: "3.9"

   services:
     rsyslog-collector:
       image: rsyslog/rsyslog-collector:latest
       environment:
         ENABLE_UDP: "on"
         ENABLE_TCP: "on"
         ENABLE_RELP: "on"
         ENABLE_TLS: "off"
       ports:
         - "514:514/udp"    # Syslog UDP
         - "514:514/tcp"    # Syslog TCP
         - "20514:2514/tcp" # RELP (external 20514 → internal 2514)
         - "6514:6514/tcp"  # TLS syslog (optional)
       volumes:
         - ./data:/var/log   # Optional: collect logs on host

Verifying the Container
-----------------------

To confirm that the collector is listening on the expected ports:

.. code-block:: bash

   docker compose exec rsyslog-collector ss -tuln

This should show listeners on ``514/udp`` and ``514/tcp`` by default, plus
``2514/tcp`` when RELP is enabled and ``6514/tcp`` when TLS is enabled.

Production Deployments
----------------------

For a complete production deployment with dashboards, alerting, and log storage,
see :doc:`../deployments/rosi_collector/index`. ROSI Collector builds on this
container image to provide Grafana visualization, Loki log storage, and
Prometheus metrics collection. In that deployment, Docker Compose mounts
additional rsyslog snippets so the collector forwards to Loki and exposes the
collector on external port ``10514`` for plain TCP clients.

.. seealso::

   - :doc:`../deployments/rosi_collector/index` - Complete log collection stack
   - `GitHub Discussions <https://github.com/rsyslog/rsyslog/discussions>`_ for community support.
   - `rsyslog Assistant AI <https://rsyslog.ai>`_ for self-help and examples.
