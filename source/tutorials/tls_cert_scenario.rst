Encrypting Syslog Traffic with TLS (SSL)
========================================

*Written by `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_
(2008-06-17)*

-  `Overview <rsyslog_secure_tls.html>`_
-  `Sample Scenario <tls_cert_scenario.html>`_
-  `Setting up the CA <tls_cert_ca.html>`_
-  `Generating Machine Certificates <tls_cert_machine.html>`_
-  `Setting up the Central Server <tls_cert_server.html>`_
-  `Setting up syslog Clients <tls_cert_client.html>`_
-  `Setting up the UDP syslog relay <tls_cert_udp_relay.html>`_
-  `Wrapping it all up <tls_cert_summary.html>`_
-  `Frequently seen Error Messages <tls_cert_errmsgs.html>`_

Sample Scenario
~~~~~~~~~~~~~~~

We have a quite simple scenario. There is one central syslog server,

named central.example.net. These server is being reported to by two
Linux machines with name zuse.example.net and turing.example.net. Also,
there is a third client - ada.example.net - which send both its own
messages to the central server but also forwards messages receive from
an UDP-only capable router. We hav decided to use ada.example.net
because it is in the same local network segment as the router and so we
enjoy TLS' security benefits for forwarding the router messages inside
the corporate network. All systems (except the router) use
`rsyslog <http://www.rsyslog.com/>`_ as the syslog software.

.. figure:: tls_cert_100.jpg
   :align: center
   :alt: 

Please note that the CA must not necessarily be connected to the rest of
the network. Actually, it may be considered a security plus if it is
not. If the CA is reachable via the regular network, it should be
sufficiently secured (firewal rules et al). Keep in mind that if the
CA's security is breached, your overall system security is breached.

In case the CA is compromised, you need to regenerate the CA's
certificate as well as all individual machines certificates.

Copyright
---------

Copyright (c) 2008 `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ and
`Adiscon <http://www.adiscon.com/en/>`_.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.2 or
any later version published by the Free Software Foundation; with no
Invariant Sections, no Front-Cover Texts, and no Back-Cover Texts. A
copy of the license can be viewed at
`http://www.gnu.org/copyleft/fdl.html <http://www.gnu.org/copyleft/fdl.html>`_.
