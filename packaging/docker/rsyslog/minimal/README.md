## rsyslog official minimal container

This container just provides a fresh rsyslog with minimal configuration
on top of ubuntu.

Use this, if you

a) want to build your own container based on current rsyslog
b) want to run rsyslog with a custom config by you

We also have a series of other official minimal containers for more
advanced use cases, e.g. gathering docker logs or acting as a central
hub.

The packaged default configuration uses ``/var/spool/rsyslog`` as the
work directory and writes to standard output via ``omstdout``.

The image includes an empty native regex lookup table at
``/etc/rsyslog/noise-drop.lkp_tbl``. Mount or replace it with an rsyslog
``type="regex"`` lookup table to drop known noisy events. The default
filter matches ``$rawmsg`` and stops processing when the lookup result is
non-empty.

This image now runs as ``syslog:adm`` by default. That fits simple
container deployments that use unprivileged ports and do not depend on
host log sockets or other root-only resources.
