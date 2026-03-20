.. _containers-user-etl:
.. _container.image.rsyslog-etl:

rsyslog/rsyslog-etl
===================

.. meta::
   :description: Reference for the rsyslog ETL container image that receives syslog and forwards events to a Vespa HTTP API.
   :keywords: rsyslog, ETL, Vespa, Docker, omhttp, collector

.. summary-start

The ``rsyslog/rsyslog-etl`` image is a specialized ingestion role that accepts syslog over UDP, TCP, and RELP, then forwards events to a Vespa HTTP endpoint using ``omhttp``.

.. summary-end

.. index::
   pair: containers; rsyslog/rsyslog-etl
   pair: Docker images; rsyslog/rsyslog-etl

Overview
--------

The ``rsyslog-etl`` image extends the standard :doc:`standard` base with a
small ETL-oriented configuration. It receives network syslog and sends each
message to a Vespa document API endpoint. The packaged rules use a simple JSON
document template containing the original message text and generate the Vespa
document path dynamically from environment variables.

This image is a concrete example of rsyslog being used as an ETL transport and
delivery component. It is optimized for Vespa-oriented pipelines, not presented
here as a generic all-destinations ETL appliance.

.. note::

   - **UDP (514/udp)** and **TCP (514/tcp)** are enabled by default.
   - **RELP (2514/tcp)** is enabled by default.
   - The packaged ETL output is **HTTP to Vespa**, not a local file sink.
   - The packaged configuration is explicitly **unencrypted** on the input side.

Environment Variables
---------------------

.. _containers-user-etl-enable_udp:
.. envvar:: ENABLE_UDP
   :noindex:

   Enable UDP syslog reception. Default ``on``.

.. _containers-user-etl-enable_tcp:
.. envvar:: ENABLE_TCP
   :noindex:

   Enable TCP syslog reception. Default ``on``.

.. _containers-user-etl-enable_relp:
.. envvar:: ENABLE_RELP
   :noindex:

   Enable RELP syslog reception on ``2514/tcp``. Default ``on``.

.. _containers-user-etl-enable_vespa:
.. envvar:: ENABLE_VESPA

   Enable the packaged Vespa HTTP action. Default ``on``.

.. _containers-user-etl-vespa_namespace:
.. envvar:: VESPA_NAMESPACE

   Vespa document namespace used in the generated REST path.

.. _containers-user-etl-vespa_doctype:
.. envvar:: VESPA_DOCTYPE

   Vespa document type used in the generated REST path.

.. _containers-user-etl-vespa_server:
.. envvar:: VESPA_SERVER

   Hostname or IP address of the Vespa HTTP endpoint.

.. _containers-user-etl-vespa_port:
.. envvar:: VESPA_PORT

   TCP port of the Vespa HTTP endpoint.

.. _containers-user-etl-rsyslog_hostname:
.. envvar:: RSYSLOG_HOSTNAME
   :noindex:

   Hostname used inside rsyslog. Defaults to the value of ``/etc/hostname`` when unset.

.. _containers-user-etl-permit_unclean_start:
.. envvar:: PERMIT_UNCLEAN_START
   :noindex:

   Skip configuration validation when set. By default ``rsyslogd -N1`` validates the configuration.

.. _containers-user-etl-rsyslog_role:
.. envvar:: RSYSLOG_ROLE
   :noindex:

   Role name consumed by the entrypoint. The current image sets this to ``collector``.

Port Mapping Reference
----------------------

+-----------+----------------+------------------+-----------------+
| Protocol  | Container Port | Example External | Controlled by   |
+===========+================+==================+=================+
| UDP Syslog | 514/udp       | 514/udp          | ``ENABLE_UDP``  |
+-----------+----------------+------------------+-----------------+
| TCP Syslog | 514/tcp       | 514/tcp          | ``ENABLE_TCP``  |
+-----------+----------------+------------------+-----------------+
| RELP       | 2514/tcp      | 2514/tcp         | ``ENABLE_RELP`` |
+-----------+----------------+------------------+-----------------+

Consumption Model
-----------------

The repository includes a sample Docker Compose deployment under
``packaging/docker/rsyslog/compose-etl/compose.yml``. That example persists the
rsyslog spool directory, exposes UDP/TCP/RELP listener ports, and passes the
Vespa connection variables as environment settings.

Related Reading
---------------

For the broader product positioning behind ETL use cases, see
:doc:`../faq/etl_tool`.
