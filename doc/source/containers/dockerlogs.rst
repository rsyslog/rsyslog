.. _containers-user-dockerlogs:
.. _container.image.rsyslog-dockerlogs:

rsyslog/rsyslog-dockerlogs
==========================

.. index::
   pair: containers; rsyslog/rsyslog-dockerlogs
   pair: Docker images; rsyslog/rsyslog-dockerlogs

This variant includes the ``imdocker`` module to read logs from the
Docker daemon and forward them to a central rsyslog instance.

Environment Variables
---------------------

.. _containers-user-dockerlogs-remote_server_name:
.. envvar:: REMOTE_SERVER_NAME

   Required hostname or IP of the collector to forward logs to.

.. _containers-user-dockerlogs-remote_server_port:
.. envvar:: REMOTE_SERVER_PORT

   TCP port on the collector. Defaults to ``514`` when unset.

.. _containers-user-dockerlogs-rsyslog_hostname:
.. envvar:: RSYSLOG_HOSTNAME
   :noindex:

   Hostname used inside rsyslog. Defaults to the value of ``/etc/hostname`` when unset.

.. _containers-user-dockerlogs-permit_unclean_start:
.. envvar:: PERMIT_UNCLEAN_START
   :noindex:

   Skip configuration validation when set. By default ``rsyslogd -N1`` validates the configuration.

.. _containers-user-dockerlogs-rsyslog_role:
.. envvar:: RSYSLOG_ROLE
   :noindex:

   Role name consumed by the entrypoint. Defaults to ``docker``.
