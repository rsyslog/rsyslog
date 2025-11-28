.. _containers-index:

Rsyslog Containers
==================

.. index::
   single: containers
   single: Docker images

Rsyslog runs well inside containers and the project provides official
Docker images for common logging scenarios. Dockerfiles and build
recipes live under `packaging/docker <https://github.com/rsyslog/rsyslog/tree/main/packaging/docker>`_.

Since version 8.32.0 rsyslog also adjusts a few defaults when it detects
that it is running as PID 1 inside a container: ``Ctrl-C`` is handled and
no pid file is written.

For complete production deployments with dashboards and monitoring, see
:doc:`../deployments/index`.

.. toctree::
   :maxdepth: 2

   user_images
   development_images
