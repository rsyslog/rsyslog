`back <rsyslog_conf_global.html>`_

$DropTrailingLFOnReception
--------------------------

**Type:** global configuration directive

**Default:** on

**Description:**

Syslog messages frequently have the line feed character (LF) as the last
character of the message. In almost all cases, this LF should not really
become part of the message. However, recent IETF syslog standardization
recommends against modifying syslog messages (e.g. to keep digital
signatures valid). This option allows to specify if trailing LFs should
be dropped or not. The default is to drop them, which is consistent with
what sysklogd does.

**Sample:**

``$DropTrailingLFOnRecption on``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
