$UMASK
------

**Type:** global configuration parameter

**Default:**

**Description:**

The $umask parameter allows to specify the rsyslogd processes' umask. If
not specified, the system-provided default is used. The value given must
always be a 4-digit octal number, with the initial digit being zero.

If $umask is specified multiple times in the configuration file, results
may be somewhat unpredictable. It is recommended to specify it only
once.

**Sample:**

``$umask 0000``

This sample removes all restrictions.

[`rsyslog site <http://www.rsyslog.com/>`_\ ]

