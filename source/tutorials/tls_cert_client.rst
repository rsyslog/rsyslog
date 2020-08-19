Setting up a client
===================

In this step, we configure a client machine. We from our scenario, we
use zuse.example.net. You need to do the same steps for all other
clients, too (in the example, that means turing.example.net). The client
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
   you load-balance to different server identities, you obviously need to
   allow all of them. It still is suggested to use explicit names.

**At this point, please be reminded once again that your security needs
may be quite different from what we assume in this tutorial. Evaluate
your options based on your security needs.**

Sample syslog.conf
~~~~~~~~~~~~~~~~~~

Keep in mind that this rsyslog.conf sends messages via TCP, only. Also,
we do not show any rules to write local files. Feel free to add them.

::

    # make gtls driver the default and set certificate files
    global(
    DefaultNetstreamDriver="gtls"
    DefaultNetstreamDriverCAFile="/path/to/contrib/gnutls/ca.pem"
    DefaultNetstreamDriverCertFile="/path/to/contrib/gnutls/cert.pem"
    DefaultNetstreamDriverKeyFile="/path/to/contrib/gnutls/key.pem"
    )

    # set up the action for all messages
    action(
    type="omfwd"
    target="central.example.net"
    protocol="tcp"
    port="6514"
    StreamDriver="gtls"
    StreamDriverMode="1" # run driver in TLS-only mode
    StreamDriverAuthMode="x509/name"
    StreamDriverPermittedPeers="central.example.net"
    )

Note: the example above forwards every message to the remote server. Of
course, you can use the normal filters to restrict the set of
information that is sent. Depending on your message volume and needs,
this may be a smart thing to do.

**Be sure to safeguard at least the private key (machine-key.pem)!** If
some third party obtains it, you security is broken!

