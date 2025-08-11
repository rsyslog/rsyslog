.. _getting_started:

Getting Started with rsyslog
############################

rsyslog is a modern, high-performance logging service and the default
system logger on many Linux distributions. It extends traditional syslog
with advanced features like structured logging, reliable TCP/TLS
delivery, and integration with modern pipelines (e.g., Elasticsearch,
Kafka, or cloud services).

This guide helps you get up and running quickly. It includes:

.. toctree::
   :maxdepth: 1

   installation
   basic_configuration
   understanding_default_config
   forwarding_logs
   next_steps

----

**Quick Start (for experienced users):**

.. code-block:: bash

   sudo apt install rsyslog
   sudo systemctl enable --now rsyslog

The following pages explain these steps in more detail.

