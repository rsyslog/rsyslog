.. _containers-rosi-loki-poc:

ROSI Loki Proof of Concept Stack
================================

The ``packaging/docker/rosi-loki`` directory ships a ready-to-run Docker
Compose stack that pairs rsyslog/ROSI with a Loki backend and Grafana for
log exploration. It targets local evaluation workflows and keeps the
configuration intentionally small so it is easy to extend for production
needs.

Requirements
------------

* Docker Engine with Docker Compose v2 support
* Open TCP ports 3000 (Grafana UI), 3100 (Loki API), and 1514 (Promtail
  syslog input)

Startup
-------

Create a dedicated working directory and initialize Docker Compose::

    cd packaging/docker/rosi-loki
    # optionally write a .env file with GRAFANA_ADMIN_PASSWORD
    docker compose up -d

The stack provisions three services on an isolated Docker network:

* ``loki`` – Grafana Loki 2.9 configured for single-process filesystem
  storage with a seven day retention policy.
* ``grafana`` – Grafana 10.4 with built-in provisioning for the Loki
  datasource and a starter "ROSI Overview" dashboard.
* ``promtail`` – Promtail 2.9 listening for syslog messages on TCP
  port 1514 and persisting its positions database on a named volume for
  durability across restarts.

Usage
-----

After the containers are healthy, send a test syslog line from the host::

    logger -n 127.0.0.1 -P 1514 -T "hello loki"

Log in to Grafana at ``http://localhost:3000`` using the ``admin``
account and the password supplied through the ``GRAFANA_ADMIN_PASSWORD``
variable (defaults to ``admin``; Grafana requires a password change on
first login). Use the Explore view with the Loki query ``{job="syslog"}``
to confirm that the message arrived.

Troubleshooting
---------------

* Check ``docker ps`` and individual container logs for startup errors.
* Ensure that local firewalls permit access to ports 3000, 3100, and
  1514/tcp.
* Review ``docker logs promtail`` for push failures and ``docker logs
  loki`` for ingestion warnings.

Next steps
----------

The PoC is a safe starting point for integrating ROSI with a modern log
query workflow. Recommended hardening tasks include enabling TLS for
Grafana and Loki, wiring rsyslog to dual-write into Loki via ``omhttp``,
and tightening password management to avoid default credentials in
shared environments.
