Encrypting Syslog Traffic with TLS (SSL)
========================================

*Written by `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_
(2008-07-03)*

-  `Overview <rsyslog_secure_tls.html>`_
-  `Sample Scenario <tls_cert_scenario.html>`_
-  `Setting up the CA <tls_cert_ca.html>`_
-  `Generating Machine Certificates <tls_cert_machine.html>`_
-  `Setting up the Central Server <tls_cert_server.html>`_
-  `Setting up syslog Clients <tls_cert_client.html>`_
-  `Setting up the UDP syslog relay <tls_cert_udp_relay.html>`_
-  `Wrapping it all up <tls_cert_summary.html>`_

Setting up a client
~~~~~~~~~~~~~~~~~~~

In this step, we configure a client machine. We from our scenario, we
use zuse.example.net. You need to do the same steps for all other
clients, too (in the example, that meanst turng.example.net). The client
check's the server's identity and talks to it only if it is the expected
server. This is a very important step. Without it, you would not detect
man-in-the-middle attacks or simple malicious servers who try to get
hold of your valuable log data.

.. figure:: tls_cert_100.jpg
   :align: center
   :alt: 

Steps to do:

-  make sure you have a functional CA (`Setting up the
   CA <tls_cert_ca.html>`_)
-  generate a machine certificate for zuse.example.net (follow
   instructions in `Generating Machine
   Certificates <tls_cert_machine.html>`_)
-  make sure you copy over ca.pem, machine-key.pem ad machine-cert.pem
   to the client. Ensure that no user except root can access them
   (**even read permissions are really bad**).
-  configure the client so that it checks the server identity and sends
   messages only if the server identity is known. Please note that you
   have the same options as when configuring a server. However, we now
   use a single name only, because there is only one central server. No
   using wildcards make sure that we will exclusively talk to that
   server (otherwise, a compromised client may take over its role). If
   you load-balance to different server identies, you obviously need to
   allow all of them. It still is suggested to use explcit names.

**At this point, please be reminded once again that your security needs
may be quite different from what we assume in this tutorial. Evaluate
your options based on your security needs.**

Sample syslog.conf
~~~~~~~~~~~~~~~~~~

Keep in mind that this rsyslog.conf sends messages via TCP, only. Also,
we do not show any rules to write local files. Feel free to add them.
````

::

    # make gtls driver the default
    $DefaultNetstreamDriver gtls

    # certificate files
    $DefaultNetstreamDriverCAFile /rsyslog/protected/ca.pem
    $DefaultNetstreamDriverCertFile /rsyslog/protected/machine-cert.pem
    $DefaultNetstreamDriverKeyFile /rsyslog/protected/machine-key.pem

    $ActionSendStreamDriverAuthMode x509/name
    $ActionSendStreamDriverPermittedPeer central.example.net
    $ActionSendStreamDriverMode 1 # run driver in TLS-only mode
    *.* @@central.example.net:10514 # forward everything to remote server

Note: the example above forwards every message to the remote server. Of
course, you can use the normal filters to restrict the set of
information that is sent. Depending on your message volume and needs,
this may be a smart thing to do.

**Be sure to safeguard at least the private key (machine-key.pem)!** If
some third party obtains it, you security is broken!

Copyright
---------

Copyright © 2008 `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ and
`Adiscon <http://www.adiscon.com/en/>`_.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.2 or
any later version published by the Free Software Foundation; with no
Invariant Sections, no Front-Cover Texts, and no Back-Cover Texts. A
copy of the license can be viewed at
`http://www.gnu.org/copyleft/fdl.html <http://www.gnu.org/copyleft/fdl.html>`_.
