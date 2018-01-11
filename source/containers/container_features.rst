Container-Features
------------------

Upon start, rsyslog checks if it is running as pid 1. If so, this is
a clear indication it is running inside a container. This changes
some parameter defaults:

- ctl-c is enabled
- no pid file is written
