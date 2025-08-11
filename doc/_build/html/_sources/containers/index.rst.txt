rsyslog and containers
======================

In this chapter, we describe how rsyslog can be used together with
containers.

All versions of rsyslog work well in containers. Versions beginning with
8.32.0 have also been made explicitly container-aware and provide some
extra features that are useful inside containers.

Note: the sources for docker containers created by the rsyslog project
can be found at https://github.com/rsyslog/rsyslog/tree/main/packaging/docker - these may
be useful as a starting point for similar efforts. Feedback, bug
reports and pull requests are also appreciated for this project.

.. toctree::
   :maxdepth: 2

   container_features
   docker_specifics
