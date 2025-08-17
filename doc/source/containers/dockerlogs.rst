.. _containers-user-dockerlogs:

rsyslog/rsyslog-dockerlogs
==========================

This variant includes the ``imdocker`` module to read logs from the
Docker daemon and forward them to a central rsyslog instance.

Environment Variables
---------------------

- ``REMOTE_SERVER_NAME`` – required hostname or IP of the collector to
  forward logs to.
- ``REMOTE_SERVER_PORT`` – TCP port on the collector. Defaults to ``514``
  when unset.
- ``RSYSLOG_HOSTNAME`` – hostname used inside rsyslog. Defaults to the
  value of ``/etc/hostname`` when unset.
- ``PERMIT_UNCLEAN_START`` – skip configuration validation when set. By
  default ``rsyslogd -N1`` validates the configuration.
- ``RSYSLOG_ROLE`` – role name consumed by the entrypoint. Defaults to
  ``docker``.
