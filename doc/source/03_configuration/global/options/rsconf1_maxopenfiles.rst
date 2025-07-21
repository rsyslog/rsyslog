$MaxOpenFiles
-------------

**Available Since:** 4.3.0

**Type:** global configuration parameter

**Default:** *operating system default*

**Description:**

Set the maximum number of files that the rsyslog process can have open
at any given time. Note that this includes open tcp sockets, so this
setting is the upper limit for the number of open TCP connections as
well. If you expect a large number of concurrent connections, it is
suggested that the number is set to the max number connected plus 1000.
Please note that each dynafile also requires up to 100 open file
handles.

The setting is similar to running "ulimit -n number-of-files".

Please note that depending on permissions and operating system
configuration, the setrlimit() request issued by rsyslog may fail, in
which case the previous limit is kept in effect. Rsyslog will emit a
warning message in this case.

**Sample:**

``$MaxOpenFiles 2000``

**Bugs:**

For some reason, this settings seems not to work on all platforms. If
you experience problems, please let us know so that we can (hopefully)
narrow down the issue.

