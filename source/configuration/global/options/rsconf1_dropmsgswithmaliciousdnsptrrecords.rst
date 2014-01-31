`back <rsyslog_conf_global.html>`_

$DropMsgsWithMaliciousDnsPTRRecords
-----------------------------------

**Type:** global configuration directive

**Default:** off

**Description:**

Rsyslog contains code to detect malicious DNS PTR records (reverse name
resolution). An attacker might use specially-crafted DNS entries to make
you think that a message might have originated on another IP address.
Rsyslog can detect those cases. It will log an error message in any
case. If this option here is set to "on", the malicious message will be
completely dropped from your logs. If the option is set to "off", the
message will be logged, but the original IP will be used instead of the
DNS name.

**Sample:**

``$DropMsgsWithMaliciousDnsPTRRecords on``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
