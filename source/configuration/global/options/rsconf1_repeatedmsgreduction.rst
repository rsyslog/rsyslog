`back <rsyslog_conf_global.html>`_

$RepeatedMsgReduction
---------------------

**Type:** global configuration directive

**Default:** depending on -e

**Description:**

This directive specifies whether or not repeated messages should be
reduced (this is the "Last line repeated n times" feature). If set to
on, repeated messages are reduced. If set to off, every message is
logged. Please note that this directive overrides the -e command line
option. In case -e is given, it is just the default value until the
first RepeatedMsgReduction directive is encountered.

This directives affects selector lines until a new directive is
specified.

**Sample:**

``$RepeatedMsgReduction off    # log every message``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
