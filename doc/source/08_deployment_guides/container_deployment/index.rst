Container Deployment
====================

Deploy rsyslog in containerized environments.

Rsyslog works well in containers. Versions beginning with 8.32.0 have also
been made explicitly container-aware and provide some extra features that
are useful inside containers.

.. note::

   The sources for docker containers created by the rsyslog project
   can be found at https://github.com/rsyslog/rsyslog/tree/main/packaging/docker
   These may be useful as a starting point for similar efforts.

.. toctree::
   :maxdepth: 2

   container_features
   docker_specifics