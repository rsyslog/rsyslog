.. _containers-user-collector:
.. _container.image.rsyslog-collector:

rsyslog/rsyslog-collector
=========================

.. index::
   pair: containers; rsyslog/rsyslog-collector
   pair: Docker images; rsyslog/rsyslog-collector

Built on the standard image, ``rsyslog-collector`` adds modules for
centralised log aggregation. It exposes TCP and UDP syslog ports and can
forward data to storage backends.

Environment Variables
---------------------

Runtime behaviour can be tuned with the following variables:

.. _containers-user-collector-enable_udp:
.. envvar:: ENABLE_UDP

   Enable UDP syslog reception. Default ``on``.

.. _containers-user-collector-enable_tcp:
.. envvar:: ENABLE_TCP

   Enable TCP syslog reception. Default ``on``.

.. _containers-user-collector-write_all_file:
.. envvar:: WRITE_ALL_FILE

   Write all messages to ``/var/log/all.log`` when ``on`` (default).

.. _containers-user-collector-write_json_file:
.. envvar:: WRITE_JSON_FILE

   Write JSON formatted messages to ``/var/log/all-json.log`` when ``on`` (default).

.. _containers-user-collector-rsyslog_hostname:
.. envvar:: RSYSLOG_HOSTNAME
   :noindex:

   Hostname used inside rsyslog. Defaults to the value of ``/etc/hostname`` when unset.

.. _containers-user-collector-permit_unclean_start:
.. envvar:: PERMIT_UNCLEAN_START
   :noindex:

   Skip configuration validation when set. By default ``rsyslogd -N1`` validates the configuration.

.. _containers-user-collector-rsyslog_role:
.. envvar:: RSYSLOG_ROLE
   :noindex:

   Role name consumed by the entrypoint. Defaults to ``collector``.
