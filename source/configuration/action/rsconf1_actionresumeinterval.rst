`back <rsyslog_conf_global.html>`_

$ActionResumeInterval
---------------------

**Type:** global configuration directive

**Default:** 30

**Description:**

Sets the ActionResumeInterval for all following actions. The interval
provided is always in seconds. Thus, multiply by 60 if you need minutes
and 3,600 if you need hours (not recommended).

When an action is suspended (e.g. destination can not be connected), the
action is resumed for the configured interval. Thereafter, it is
retried. If multiple retires fail, the interval is automatically
extended. This is to prevent excessive ressource use for retires. After
each 10 retries, the interval is extended by itself. To be precise, the
actual interval is (numRetries / 10 + 1) \* $ActionResumeInterval. so
after the 10th try, it by default is 60 and after the 100th try it is
330.

**Sample:**

$ActionResumeInterval 30 

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
