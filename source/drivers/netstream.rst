`back <rsyslog_conf_global.html>`_

Network Stream Drivers
======================

Network stream drivers are a layer between various parts of rsyslogd
(e.g. the imtcp module) and the transport layer. They provide sequenced
delivery, authentication and confidentiality to the upper layers.
Drivers implement different capabilities.

Users need to know about netstream drivers because they need to
configure the proper driver, and proper driver properties, to achieve
desired results (e.g. a `TLS-protected syslog
transmission <rsyslog_tls.html>`_).

The following drivers exist:

-  `ptcp <ns_ptcp.html>`_ - the plain tcp network transport (no
   security)
-  `gtls <ns_gtls.html>`_ - a secure TLS transport implemented via the
   GnuTLS library

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
