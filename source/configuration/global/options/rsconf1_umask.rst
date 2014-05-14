$UMASK
------

**Type:** global configuration directive

**Default:**

**Description:**

The $umask directive allows to specify the rsyslogd processes' umask. If
not specified, the system-provided default is used. The value given must
always be a 4-digit octal number, with the initial digit being zero.

If $umask is specified multiple times in the configuration file, results
may be somewhat unpredictable. It is recommended to specify it only
once.

**Sample:**

``$umask 0000``

This sample removes all restrictions.

[`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2007-2014 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
