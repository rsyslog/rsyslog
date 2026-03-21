.. _containers-user-dockerlogs:
.. _container.image.rsyslog-dockerlogs:

rsyslog/rsyslog-dockerlogs
==========================

.. meta::
   :description: Reference for the rsyslog Docker log forwarding image based on imdocker and omfwd.
   :keywords: rsyslog, dockerlogs, imdocker, Docker, forwarding

.. summary-start

The ``rsyslog/rsyslog-dockerlogs`` image reads Docker log events with ``imdocker`` and forwards them to a central rsyslog collector over TCP.

.. summary-end

.. index::
   pair: containers; rsyslog/rsyslog-dockerlogs
   pair: Docker images; rsyslog/rsyslog-dockerlogs

This variant includes the ``imdocker`` module to read logs from the
Docker daemon and forward them to a central rsyslog instance. The packaged
configuration uses ``omfwd`` over TCP with an in-memory queue and infinite
retry so temporary collector outages do not immediately drop events.

The packaged dockerlogs image still runs as root by default. Typical
deployments rely on Docker daemon or socket access, which is commonly
root-scoped.

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
