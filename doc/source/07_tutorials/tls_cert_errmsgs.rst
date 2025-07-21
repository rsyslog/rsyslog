Error Messages
==============

This page covers error message you may see when setting up
`rsyslog <http://www.rsyslog.com>`_ with TLS. Please note that many of
the message stem back to the TLS library being used. In those cases,
there is not always a good explanation available in rsyslog alone.

A single error typically results in two or more message being emitted:
(at least) one is the actual error cause, followed by usually one
message with additional information (like certificate contents). In a
typical system, these message should immediately follow each other in
your log. Keep in mind that they are reported as syslog.err, so you need
to capture these to actually see errors (the default rsyslog.conf's
shipped by many systems will do that, recording them e.g. in
/etc/messages).

certificate invalid
-------------------

Sample:

::

  not permitted to talk to peer, certificate invalid: insecure algorithm

This message may occur during connection setup. It indicates that the
remote peer's certificate can not be accepted. The reason for this is
given in the message part that is shown in red. Please note that this
red part directly stems back to the TLS library, so rsyslog does
actually not have any more information about the reason.

With GnuTLS, the following reasons have been seen in practice:

insecure algorithm
^^^^^^^^^^^^^^^^^^

The certificate contains information on which encryption algorithms are
to be used. This information is entered when the certificate is created.
Some older algorithms are no longer secure and the TLS library does not
accept them. Thus the connection request failed. The cure is to use a
certificate with sufficiently secure algorithms.

Please note that no encryption algorithm is totally secure. It only is
secure based on our current knowledge AND on computing power available.
As computers get more and more powerful, previously secure algorithms
become insecure over time. As such, algorithms considered secure today
may not be accepted by the TLS library in the future.

So in theory, after a system upgrade, a connection request may fail with
the "insecure algorithm" failure without any change in rsyslog
configuration or certificates. This could be caused by a new perception
of the TLS library of what is secure and what not.

GnuTLS error -64
----------------

Sample:

::

  unexpected GnuTLS error -64 in nsd_gtls.c:517: Error while reading file.

This error points to an encoding error with the pem file in question.
It means "base 64 encoding error". From my experience, it can be caused
by a couple of things, some of them not obvious:

-  You specified a wrong file, which is not actually in .pem format
-  The file was incorrectly generated
-  I think I have also seen this when I accidentally swapped private key
   files and certificate files. So double-check the type of file you are
   using.
-  It may even be a result of an access (permission) problem. In theory,
   that should lead to another error, but in practice it sometimes seems
   to lead to this -64 error.

info on invalid cert
--------------------

Sample:

::

  info on invalid cert: peer provided 1 certificate(s). Certificate 1 info: certificate valid from Wed Jun 18 11:45:44 2008 to Sat Jun 16 11:45:53 2018; Certificate public key: RSA; DN: C=US,O=Sample Corp,OU=Certs,L=Somewhere,ST=CA,CN=somename; Issuer DN: C=US,O=Sample Corp,OU=Certs,L=Somewhere,ST=CA,CN=somename,EMAIL=xxx@example.com; SAN:DNSname: machine.example.net;

This is **not** an error message in itself. It always follows the actual
error message and tells you what is seen in the peer's certificate. This
is done to give you a chance to evaluate the certificate and better
understand why the initial error message was issued.

Please note that you can NOT diagnose problems based on this message
alone. It follows in a number of error cases and does not pinpoint any
problems by itself.

invalid peer name
-----------------

Sample:

::

  # sender error
  error: peer name not authorized -  not permitted to talk to it. Names: DNSname: syslog.example.com; CN: syslog.example.com;

  # receiver error
  unexpected GnuTLS error -110 in nsd_gtls.c:536: The TLS connection was non-properly terminated.
  netstream session 0x7fee240ef650 from X.X.X.X will be closed due to error

This error is caused by the Subject Alternative Name or Common Name on the receivers certificate not matching any StreamDriverPermittedPeers (RainerScript) / $ActionSendStreamDriverPermittedPeers (Legacy Rsyslog). Either we need to update this parameter with the correct domain name / appropriate wildcard or change the AuthMode of the Stream Driver to be less strict (be sure to carefully read the documentation and understand the implications before taking this route).

The receiver error is fairly generic and comes from the upstream GnuTLS library, because the sender has decided it's not authorized to talk to the remote peer over TLS it tears down the connection before any records are sent, this is treated as a premature termination by the library and it returns the given error. 
