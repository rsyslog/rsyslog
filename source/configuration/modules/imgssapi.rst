`back <rsyslog_conf_modules.html>`_

imgssapi: GSSAPI Syslog Input Module
====================================

**Module Name:    imgssapi**

**Author:**\ varmojfekoj

**Description**:

.. toctree::
   :maxdepth: 1

   gssapi

Provides the ability to receive syslog messages from the network
protected via Kerberos 5 encryption and authentication. This module also
accept plain tcp syslog messages on the same port if configured to do
so. If you need just plain tcp, use :doc:`imtcp <imtcp>` instead.

**Configuration Directives**:

-  InputGSSServerRun <port>
    Starts a GSSAPI server on selected port - note that this runs
   independently from the TCP server.
-  InputGSSServerServiceName <name>
    The service name to use for the GSS server.
-  $InputGSSServerPermitPlainTCP on\|off
    Permits the server to receive plain tcp syslog (without GSS) on the
   same port
-  $InputGSSServerMaxSessions <number>
    Sets the maximum number of sessions supported

**Caveats/Known Bugs:**

-  module always binds to all interfaces
-  only a single listener can be bound

**Sample:**

This sets up a GSS server on port 1514 that also permits to receive
plain tcp syslog messages (on the same port):

$ModLoad imgssapi # needs to be done just once $InputGSSServerRun 1514
$InputGSSServerPermitPlainTCP on

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2011 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
