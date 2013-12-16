`back to rsyslog module overview <rsyslog_conf_modules.html>`_

UDP Syslog Input Module
=======================

**Module Name:    imudp**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Multi-Ruleset Support:**\ since 5.3.2

**Description**:

Provides the ability to receive syslog messages via UDP.

Multiple receivers may be configured by specifying $UDPServerRun
multiple times.

**Configuration Directives**:

-  $UDPServerAddress <IP>
    local IP address (or name) the UDP listens should bind to
-  $UDPServerRun <port>
    former -r<port> option, default 514, start UDP server on this port,
   "\*" means all addresses
-  $UDPServerTimeRequery <nbr-of-times>
    this is a performance optimization. Getting the system time is very
   costly. With this setting, imudp can be instructed to obtain the
   precise time only once every n-times. This logic is only activated if
   messages come in at a very fast rate, so doing less frequent time
   calls should usually be acceptable. The default value is two, because
   we have seen that even without optimization the kernel often returns
   twice the identical time. You can set this value as high as you like,
   but do so at your own risk. The higher the value, the less precise
   the timestamp.
-  $InputUDPServerBindRuleset <ruleset>
    Binds the listener to a specific `ruleset <multi_ruleset.html>`_.

**Caveats/Known Bugs:**

-  currently none known

**Sample:**

This sets up an UPD server on port 514:

$ModLoad imudp # needs to be done just once $UDPServerRun 514

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2009 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
