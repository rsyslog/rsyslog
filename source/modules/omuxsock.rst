Unix sockets Output Module (omuxsock)
=====================================

**Module Name:    omuxsock**

**Available since:   ** 4.7.3, 5.5.7

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module supports sending syslog messages to local Unix sockets. Thus
it provided a fast message-passing interface between different rsyslog
instances. The counterpart to omuxsock is `imuxsock <imuxsock.html>`_.
Note that the template used together with omuxsock must be suitable to
be processed by the receiver.

**Configuration Directives**:

-  **$OMUxSockSocket**
    Name of the socket to send data to. This has no default and **must**
   be set.
-  **$OMUxSockDefaultTemplate**
    This can be used to override the default template to be used
   together with omuxsock. This is primarily useful if there are many
   forwarding actions and each of them should use the same template.

**Caveats/Known Bugs:**

Currently, only datagram sockets are supported.

**Sample:**

The following sample writes all messages to the "/tmp/socksample"
socket.

$ModLoad omuxsock $OMUxSockSocket /tmp/socksample \*.\* :omuxsock:
[`manual index <manual.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2010 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
