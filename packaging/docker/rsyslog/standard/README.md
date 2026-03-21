## rsyslog official standard container

This image builds on the minimal variant and includes commonly used rsyslog modules for a balanced default setup.

Like the minimal image, it runs as ``syslog:adm`` by default. That is
appropriate for the packaged HTTP-capable base role, which does not bind
privileged ports in its shipped configuration.

See the [main README](../../README.md) for build instructions and details on the image variants.
