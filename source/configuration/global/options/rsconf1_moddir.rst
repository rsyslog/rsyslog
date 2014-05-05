`back <rsyslog_conf_global.html>`_

$ModDir
-------

**Type:** global configuration directive

**Default:** system default for user libraries, e.g.
/usr/local/lib/rsyslog/

**Description:**

Provides the default directory in which loadable modules reside. This
may be used to specify an alternate location that is not based on the
system default. If the system default is used, there is no need to
specify this directive. Please note that it is vitally important to end
the path name with a slash, else module loads will fail.

**Sample:**

``$ModDir /usr/rsyslog/libs/  # note the trailing slash!``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
