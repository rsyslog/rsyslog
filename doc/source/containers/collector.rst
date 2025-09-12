.. _containers-user-collector:
.. _container.image.rsyslog-collector:

rsyslog/rsyslog-collector
=========================

.. index::
   pair: containers; rsyslog/rsyslog-collector
   pair: Docker images; rsyslog/rsyslog-collector

Overview
--------

The ``rsyslog-collector`` container image extends the standard
:doc:`standard` base with modules for **centralised log aggregation**.
It is preconfigured to receive logs via UDP, TCP, and optionally RELP, and can
forward them to storage backends or files.

This image is the recommended starting point for building a log
collector or relay service.

.. note::

   - **UDP (514/udp)** and **TCP (514/tcp)** are enabled by default.  
   - **RELP (2514/tcp)** is available but **disabled by default**.  
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
       ports:
         - "514:514/udp"    # Syslog UDP
         - "514:514/tcp"    # Syslog TCP
         - "20514:2514/tcp" # RELP (external 20514 → internal 2514)
       volumes:
         - ./data:/var/log   # Optional: collect logs on host

Verifying the Container
-----------------------

To confirm that the collector is listening on the expected ports:

.. code-block:: bash

   docker compose exec rsyslog-collector ss -tuln

This should show listeners on ``514/udp``, ``514/tcp``, and ``2514/tcp`` when RELP is enabled.

.. seealso::

   - `GitHub Discussions <https://github.com/rsyslog/rsyslog/discussions>`_ for community support.
   - `rsyslog Assistant AI <https://rsyslog.ai>`_ for self-help and examples.

