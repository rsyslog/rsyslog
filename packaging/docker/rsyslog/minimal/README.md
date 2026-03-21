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

This image now runs as ``syslog:adm`` by default. That fits simple
container deployments that use unprivileged ports and do not depend on
host log sockets or other root-only resources.
