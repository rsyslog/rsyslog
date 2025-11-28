.. _getting_started:

Getting Started with rsyslog
############################

rsyslog is a modern, high-performance logging service and the default
system logger on many Linux distributions. It extends traditional syslog
with advanced features like structured logging, reliable TCP/TLS
delivery, and integration with modern pipelines (e.g., Elasticsearch,
Kafka, or cloud services).

This guide helps you get up and running quickly. For a complete
production deployment with dashboards and monitoring, see
:doc:`../deployments/rosi_collector/index`.

It includes:

.. toctree::
   :maxdepth: 1

   beginner_tutorials/index
   ai-assistants
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

.. _getting-started-versioning:

Understanding rsyslog version numbers
=====================================

Every regular rsyslog release is tagged ``8.yymm.0``. The constant ``8`` marks
the current major series, ``yymm`` tells you the year and month of the build,
and the trailing digit starts at ``0`` and only increments when a quick
follow-up fix ships. Seeing a version such as ``8.2404.0`` instantly reveals it
was published in April 2024 and is part of the same feature level as any other
``8.2404.x`` package. Stable builds arrive roughly every second month in even
numbered months, typically on a Tuesday around mid-month, with an earlier
December drop to avoid holiday downtime. If your installation shows a much
older ``yymm`` value, plan an upgrade to keep up with supported features and
fixes.

Newcomers can read the full background and rationale in
:ref:`about-release-versioning`.
