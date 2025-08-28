.. _containers-user-standard:
.. _container.image.rsyslog-standard:

rsyslog/rsyslog
===============

.. index::
   pair: containers; rsyslog/rsyslog
   pair: Docker images; rsyslog/rsyslog

The general-purpose image builds on ``rsyslog-minimal`` and adds commonly
used modules such as ``imhttp`` and ``omhttp``. Use it when you need a
ready-to-run rsyslog with HTTP ingestion or forwarding capabilities.

Environment Variables
---------------------

The same entrypoint variables as the minimal image are available:

.. _containers-user-standard-rsyslog_hostname:
.. envvar:: RSYSLOG_HOSTNAME
   :noindex:

   Hostname used inside rsyslog. Defaults to the value of ``/etc/hostname`` when unset.

.. _containers-user-standard-permit_unclean_start:
.. envvar:: PERMIT_UNCLEAN_START
   :noindex:

   Skip configuration validation when set. By default ``rsyslogd -N1`` validates the configuration.

.. _containers-user-standard-rsyslog_role:
.. envvar:: RSYSLOG_ROLE
   :noindex:

   Role name consumed by the entrypoint. Defaults to ``standard``.
