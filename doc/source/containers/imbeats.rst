.. _containers-sample-imbeats:
.. _container.sample.rsyslog-imbeats:

rsyslog/rsyslog-imbeats sample
==============================

.. meta::
   :description: Sample rsyslog container definition for receiving Elastic Agent and Filebeat Logstash output with imbeats.
   :keywords: rsyslog, containers, Docker, imbeats, Elastic Agent, Filebeat, Logstash output, Beats, TLS

.. summary-start

The ``rsyslog/rsyslog-imbeats`` sample container definition receives Elastic
Agent and Filebeat ``output.logstash`` traffic with ``imbeats`` on port
``5044``. It is a repository sample and is not part of the published rsyslog
container image family yet.

.. summary-end

.. index::
   pair: containers; rsyslog-imbeats sample
   pair: Docker images; imbeats sample

Status
------

The sample files live in ``packaging/docker/rsyslog/imbeats``. They are not
wired into the container ``Makefile``, release builds, Docker Hub metadata, or
``latest`` tagging. Use them as a concrete starting point when you want to run
an ``imbeats`` receiver in a container.

The sample assumes that the base image or package source provides the package
containing ``imbeats.so``. It installs ``rsyslog-gnutls`` as a concrete TLS
stream-driver package example.

Local build
-----------

Build the sample directly from its directory:

.. code-block:: bash

   docker build \
     -t rsyslog-imbeats-sample:local \
     packaging/docker/rsyslog/imbeats

This direct build is separate from the official container image Makefile.

Docker Compose example
----------------------

.. code-block:: yaml

   services:
     rsyslog-imbeats:
       image: rsyslog-imbeats-sample:local
       ports:
         - "5044:5044/tcp"
       environment:
         IMBEATS_PORT: "5044"
         TLS_AUTH_MODE: "anon"
         TLS_CA_FILE: /etc/rsyslog/tls/ca.pem
         TLS_CERT_FILE: /etc/rsyslog/tls/server-cert.pem
         TLS_KEY_FILE: /etc/rsyslog/tls/server-key.pem
         IMBEATS_OUTPUT_FILE: /var/log/imbeats.log
       volumes:
         - ./certs:/etc/rsyslog/tls:ro
         - ./logs:/var/log

Elastic Agent output
--------------------

Configure Elastic Agent to send Logstash output to the container:

.. code-block:: yaml

   outputs:
     default:
       type: logstash
       hosts: ["rsyslog-imbeats.example.net:5044"]
       compression_level: 9
       ssl.enabled: true
       ssl.certificate_authorities:
         - /etc/elastic-agent/certs/ca.pem

For Filebeat standalone configuration, use the same settings under
``output.logstash``.

Operational notes
-----------------

- The container listens on ``5044/tcp`` by default.
- TLS is configured through the mounted certificate paths. Install and load a
  TLS stream-driver package in images derived from this sample.
- The default ``TLS_AUTH_MODE=anon`` lets Elastic Agent or Filebeat verify the
  rsyslog server certificate without requiring a client certificate. Use a
  stricter certificate-validation auth mode only after configuring client
  certificates on the sender.
- Production deployments should use certificate verification. Avoid disabling
  verification except for isolated tests.
- The sample writes received event JSON to ``/var/log/imbeats.log``. Mount a
  custom rsyslog snippet when you want to forward to another destination.

.. seealso::

   - :doc:`../configuration/modules/imbeats` - imbeats module reference
   - :doc:`user_images` - rsyslog user-focused container images
