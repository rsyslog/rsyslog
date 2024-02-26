`rsyslog.conf configuration parameter <rsyslog_conf_global.html>`_

$AbortOnUncleanConfig
----------------------

**Type:** global configuration parameter

**Parameter Values:** boolean (on/off, yes/no)

**Available since:** 5.3.1+

**Default:** off

**Description:**

This parameter permits to prevent rsyslog from running when the
configuration file is not clean. "Not Clean" means there are errors or
some other annoyances that rsyslogd reports on startup. This is a
user-requested feature to have a strict startup mode. Note that with the
current code base it is not always possible to differentiate between an
real error and a warning-like condition. As such, the startup will also
prevented if warnings are present. I consider this a good thing in being
"strict", but I admit there also currently is no other way of doing it.

It is recommended to use the new config style. The equivalent of this
parameter in the new style is ``global(abortOnUncleanConfig="")``.

**Caveats:**

Note that the consequences of a failed rsyslogd startup can be much more
serious than a startup with only partial configuration. For example, log
data may be lost or systems that depend on the log server in question
will not be able to send logs, what in the ultimate result could result
in a system hang on those systems. Also, the local system may hang when
the local log socket has become full and is not read. There exist many
such scenarios. As such, it is strongly recommended not to turn on this
parameter.

[`rsyslog site <http://www.rsyslog.com/>`_\ ]

