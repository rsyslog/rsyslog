`back to rsyslog module documentation <rsyslog_conf_modules.html>`_

RELP Output Module (omrelp)
===========================

**Module Name:    omrelp**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module supports sending syslog messages over the reliable RELP
protocol. For RELP's advantages over plain tcp syslog, please see the
documentation for `imrelp <imrelp.html>`_ (the server counterpart). 

Setup

Please note the `librelp <http://www.librelp.com>`_ is required for
imrelp (it provides the core relp protocol implementation).

**Configuration Directives**:

This module uses old-style action configuration to keep consistent with
the forwarding rule. So far, no additional configuration directives can
be specified. To send a message via RELP, use

\*.\*  :omrelp:<sever>:<port>;<template>

just as you use 

\*.\*  @@<sever>:<port>;<template>

to forward a message via plain tcp syslog.

**Caveats/Known Bugs:**

See `imrelp <imrelp.html>`_, which documents them. 

**Sample:**

The following sample sends all messages to the central server
"centralserv" at port 2514 (note that that server must run imrelp on
port 2514). Rsyslog's high-precision timestamp format is used, thus the
special "RSYSLOG\_ForwardFormat" (case sensitive!) template is used.

$ModLoad omrelp # forward messages to the remote server "myserv" on #
port 2514 \*.\* :omrelp:centralserv:2514;RSYSLOG\_ForwardFormat Note: to
use IPv6 addresses, encode them in [::1] format.

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
