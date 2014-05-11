`rsyslog.conf configuration directive <rsyslog_conf_global.html>`_

$AbortOnUncleanConfig
----------------------

**Type:** global configuration directive

**Parameter Values:** boolean (on/off, yes/no)

**Available since:** 5.3.1+

**Default:** off

**Description:**

This directive permits to prevent rsyslog from running when the
configuration file is not clean. "Not Clean" means there are errors or
some other annoyances that rsyslgod reports on startup. This is a
user-requested feature to have a strict startup mode. Note that with the
current code base it is not always possible to differentiate between an
real error and a warning-like condition. As such, the startup will also
prevented if warnings are present. I consider this a good thing in being
"strict", but I admit there also currently is no other way of doing it.

**Caveats:**

Note that the consequences of a failed rsyslogd startup can be much more
serious than a startup with only partial configuration. For example, log
data may be lost or systems that depend on the log server in question
will not be able to send logs, what in the ultimate result could result
in a system hang on those systems. Also, the local system may hang when
the local log socket has become full and is not read. There exist many
such scenarios. As such, it is strongly recommended not to turn on this
directive.

[`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2009-2014 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
