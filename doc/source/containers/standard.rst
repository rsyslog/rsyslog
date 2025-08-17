.. _containers-user-standard:

rsyslog/rsyslog
===============

The general-purpose image builds on ``rsyslog-minimal`` and adds commonly
used modules such as ``imhttp`` and ``omhttp``. Use it when you need a
ready-to-run rsyslog with HTTP ingestion or forwarding capabilities.

Environment Variables
---------------------

The same entrypoint variables as the minimal image are available:

- ``RSYSLOG_HOSTNAME`` – hostname used inside rsyslog. Defaults to the
  value of ``/etc/hostname`` when unset.
- ``PERMIT_UNCLEAN_START`` – skip configuration validation when set. By
  default ``rsyslogd -N1`` validates the configuration.
- ``RSYSLOG_ROLE`` – role name consumed by the entrypoint. Defaults to
  ``standard``.
