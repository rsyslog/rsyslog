Encrypting Syslog Traffic with TLS (SSL)
========================================

Written by `Rainer Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ (2008-07-03)

.. toctree::
   :maxdepth: 1

   secure_tls
   tls_cert_scenario
   tls_cert_ca
   tls_cert_machine
   tls_cert_server
   tls_cert_client
   tls_cert_udp_relay
   tls_cert_errmsgs

Summary
~~~~~~~

If you followed the steps outlined in this documentation set, you now
have

a reasonable (for most needs) secure setup for the following
environment:

.. figure:: tls_cert_100.jpg
   :align: center
   :alt: 

You have learned about the security decisions involved and which we made
in this example. **Be once again reminded that you must make sure
yourself that whatever you do matches your security needs!** There is no
guarantee that what we generally find useful actually is. It may even be
totally unsuitable for your environment.

In the example, we created a rsyslog certificate authority (CA). Guard
the CA's files. You need them whenever you need to create a new machine
certificate. We also saw how to generate the machine certificates
themselfs and distribute them to the individual machines. Also, you have
found some configuration samples for a sever, a client and a syslog
relay. Hopefully, this will enable you to set up a similar system in
many environments.

Please be warned that you defined some expiration dates for the
certificates. After they are reached, the certificates are no longer
valid and rsyslog will NOT accept them. At that point, syslog messages
will no longer be transmitted (and rsyslogd will heavily begin to
complain). So it is a good idea to make sure that you renew the
certificates before they expire. Recording a reminder somewhere is
probably a good idea.

If you have any more questions, please visit the `rsyslog
forum <http://kb.monitorware.com/rsyslog-f40.html>`_ and simply ask ;)

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
