`back <rsyslog_conf_global.html>`_

$FailOnChownFailure
-------------------

**Type:** global configuration directive

**Default:** on

**Description:**

This option modifies behaviour of dynaFile creation. If different owners
or groups are specified for new files or directories and rsyslogd fails
to set these new owners or groups, it will log an error and NOT write to
the file in question if that option is set to "on". If it is set to
"off", the error will be ignored and processing continues. Keep in mind,
that the files in this case may be (in)accessible by people who should
not have permission. The default is "on".

**Sample:**

``$FailOnChownFailure off``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
