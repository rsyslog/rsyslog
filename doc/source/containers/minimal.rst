.. _containers-user-minimal:
.. _container.image.rsyslog-minimal:

rsyslog/rsyslog-minimal
=======================

.. index::
   pair: containers; rsyslog/rsyslog-minimal
   pair: Docker images; rsyslog/rsyslog-minimal

A lean Ubuntu-based image containing the rsyslog core and a tiny
configuration that writes logs to standard output. It serves as the
foundation for the other rsyslog images and is suitable when you want to
add your own modules or configuration.

Runtime Notes
-------------

The packaged default configuration uses ``/var/spool/rsyslog`` as the
work directory and writes to standard output via ``omstdout``.

The image also includes an empty native regex lookup table at
``/etc/rsyslog/noise-drop.lkp_tbl``. Replace or mount this file to drop
known noisy events before packaged outputs process them. The shipped
filter matches against ``$rawmsg`` because it preserves the full received
event and is usually the most robust property for source-side filtering.
If a regex table entry matches and returns any non-empty tag, rsyslog
calls ``stop`` for that event. To match another property, such as
``$msg``, replace the shipped ``/etc/rsyslog.d/02-noise-drop.conf``
snippet with custom rsyslog configuration.

The lookup table uses the standard rsyslog lookup-table JSON format and
POSIX extended regular expressions. The first matching entry wins:

.. code-block:: json

   {
     "version": 1,
     "nomatch": "",
     "type": "regex",
     "table": [
       { "regex": "healthcheck succeeded", "tag": "drop" }
     ]
   }

The image runs as ``syslog:adm`` by default. That fits simple container
deployments that use unprivileged ports and do not depend on privileged
inputs such as host log sockets or other root-only resources.

Environment Variables
---------------------

The entrypoint script recognizes the following variables:

.. _containers-user-minimal-rsyslog_hostname:
.. envvar:: RSYSLOG_HOSTNAME

   Hostname used inside rsyslog. Defaults to the value of ``/etc/hostname`` when unset.

.. _containers-user-minimal-permit_unclean_start:
.. envvar:: PERMIT_UNCLEAN_START

   Skip configuration validation when set. By default ``rsyslogd -N1`` validates the configuration.

.. _containers-user-minimal-rsyslog_role:
.. envvar:: RSYSLOG_ROLE

   Role name consumed by the entrypoint. Defaults to ``minimal`` and normally does not need to be changed.
