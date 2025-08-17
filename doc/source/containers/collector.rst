.. _containers-user-collector:

rsyslog/rsyslog-collector
=========================

Built on the standard image, ``rsyslog-collector`` adds modules for
centralised log aggregation. It exposes TCP and UDP syslog ports and can
forward data to storage backends.

Environment Variables
---------------------

Runtime behaviour can be tuned with the following variables:

- ``ENABLE_UDP`` – enable UDP syslog reception. Default ``on``.
- ``ENABLE_TCP`` – enable TCP syslog reception. Default ``on``.
- ``WRITE_ALL_FILE`` – write all messages to ``/var/log/all.log`` when
  ``on`` (default).
- ``WRITE_JSON_FILE`` – write JSON formatted messages to
  ``/var/log/all-json.log`` when ``on`` (default).
- ``RSYSLOG_HOSTNAME`` – hostname used inside rsyslog. Defaults to the
  value of ``/etc/hostname`` when unset.
- ``PERMIT_UNCLEAN_START`` – skip configuration validation when set. By
  default ``rsyslogd -N1`` validates the configuration.
- ``RSYSLOG_ROLE`` – role name consumed by the entrypoint. Defaults to
  ``collector``.
