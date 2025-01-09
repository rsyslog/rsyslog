SSL Encrypting Syslog with Stunnel
==================================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net/>`_ *(2005-07-22)*

**HISTORICAL DOCUMENT**

**Note: this is an outdated HISTORICAL document.** A much better description on
`securing syslog with TLS  <http://www.rsyslog.com/doc/master/tutorials/tls_cert_summary.html>`_
is available.

Abstract
--------

**In this paper, I describe how to encrypt**
`syslog <http://www.monitorware.com/en/topics/syslog/>`_ **messages on the
network.** Encryption is vital to keep the confidential content of
syslog messages secure. I describe the overall approach and provide a
HOWTO do it with the help of `rsyslogd <http://www.rsyslog.com>`_ and
`stunnel <http://www.stunnel.org>`_.*

Please note that starting with rsyslog 3.19.0, `rsyslog provides native
TLS/SSL encryption <rsyslog_tls.html>`_ without the need of stunnel. I
strongly recommend to use that feature instead of stunnel. The stunnel
documentation here is mostly provided for backwards compatibility. New
deployments are advised to use native TLS mode.\ **

Background
----------

**Syslog is a clear-text protocol. That means anyone with a sniffer can
have a peek at your data.** In some environments, this is no problem at
all. In others, it is a huge setback, probably even preventing
deployment of syslog solutions. Thankfully, there is an easy way to
encrypt syslog communication. I will describe one approach in this
paper.

The most straightforward solution would be that the syslogd itself
encrypts messages. Unfortunately, encryption is only standardized in
`RFC 3195 <http://www.monitorware.com/Common/en/glossary/rfc3195.php>`_.
But there is currently no syslogd that implements RFC 3195's encryption
features, so this route leads to nothing. Another approach would be to
use vendor- or project-specific syslog extensions. There are a few
around, but the problem here is that they have compatibility issues.
However, there is one surprisingly easy and interoperable solution:
though not standardized, many vendors and projects implement plain tcp
syslog. In a nutshell, plain tcp syslog is a mode where standard syslog
messages are transmitted via tcp and records are separated by newline
characters. This mode is supported by all major syslogd's (both on
Linux/Unix and Windows) as well as log sources (for example,
`EventReporter <http://www.eventreporter.com/en/>`_ for Windows Event
Log forwarding). Plain tcp syslog offers reliability, but it does not
offer encryption in itself. However, since it operates on a tcp stream,
it is now easy to add encryption. There are various ways to do that. In
this paper, I will describe how it is done with stunnel (an other
alternative would be `IPsec <https://en.wikipedia.org/wiki/IPsec>`_, for
example).

Stunnel is open source and it is available both for Unix/Linux and
Windows. It provides a way to use ssl communication for any non-ssl
aware client and server - in this case, our syslogd.

Stunnel works much like a wrapper. Both on the client and on the server
machine, tunnel portals are created. The non-ssl aware client and server
software is configured to not directly talk to the remote partner, but
to the local (s)tunnel portal instead. Stunnel, in turn, takes the data
received from the client, encrypts it via ssl, sends it to the remote
tunnel portal and that remote portal sends it to the recipient process
on the remote machine. The transfer to the portals is done via
unencrypted communication. As such, it is vital that the portal and the
respective program that is talking to it are on the same machine,
otherwise data would travel partly unencrypted. Tunneling, as done by
stunnel, requires connection oriented communication. This is why you
need to use tcp-based syslog. As a side-note, you can also encrypt a
plain-text RFC 3195 session via stunnel, though this definitely is not
what the protocol designers had on their mind ;)

In the rest of this document, I assume that you use rsyslog on both the
client and the server. For the samples, I use
`Debian <https://www.debian.org/>`_. Interestingly, there are some
annoying differences between stunnel implementations. For example, on
Debian a comment line starts with a semicolon (';'). On `Red
Hat <https://www.redhat.com/en>`_, it starts with a hash sign ('#'). So you
need to watch out for subtle issues when setting up your system.

Overall System Setup
--------------------

In this paper, I assume two machines, one named "client" and the other
named "server". It is obvious that, in practice, you will probably have
multiple clients but only one server. Syslog traffic shall be
transmitted via stunnel over the network. Port 60514 is to be used for
that purpose. The machines are set up as follows:

**Client**

-  rsyslog forwards  message to stunnel local portal at port 61514
-  local stunnel forwards data via the network to port 60514 to its
   remote peer

**Server**

-  stunnel listens on port 60514 to connections from its client peers
-  all connections are forwarded to the locally-running rsyslog
   listening at port 61514

Setting up the system
---------------------

For Debian, you need the "stunnel4" package. The "stunnel" package is
the older 3.x release, which will not support the configuration I
describe below. Other distributions might have other names. For example,
on Red Hat it is just "stunnel". Make sure that you install the
appropriate package on both the client and the server. It is also a good
idea to check if there are updates for either stunnel or openssl (which
stunnel uses) - there are often security fixes available and often the
latest fixes are not included in the default package.

In my sample setup, I use only the bare minimum of options. For example,
I do not make the server check client certificates. Also, I do not talk
much about certificates at all. If you intend to really secure your
system, you should probably learn about certificates and how to manage
and deploy them. This is beyond the scope of this paper. For additional
information,
`http://www.stunnel.org/faq/certs.html <http://www.stunnel.org/faq/certs.html>`_
is a good starting point.

You also need to install rsyslogd on both machines. Do this before
starting with the configuration. You should also familiarize yourself
with its configuration file syntax, so that you know which actions you
can trigger with it. Rsyslogd can work as a drop-in replacement for
stock `sysklogd <http://www.infodrom.org/projects/sysklogd/>`_. So if
you know the standard syslog.conf syntax, you do not need to learn any
more to follow this paper.

Server Setup
~~~~~~~~~~~~

At the server, you need to have a digital certificate. That certificate
enables SSL operation, as it provides the necessary crypto keys being
used to secure the connection. Many versions of stunnel come with a
default certificate, often found in /etc/stunnel/stunnel.pem. If you
have it, it is good for testing only. If you use it in production, it is
very easy to break into your secure channel as everybody is able to get
hold of your private key. I didn't find an stunnel.pem on my Debian
machine. I guess the Debian folks removed it because of its insecurity.

You can create your own certificate with a simple openssl tool - you
need to do it if you have none and I highly recommend to create one in
any case. To create it, cd to /etc/stunnel and type:

    ``openssl req -new -x509 -days 3650 -nodes -out  stunnel.pem -keyout stunnel.pem``

That command will ask you a number of questions. Provide some answer for
them. If you are unsure, read
`http://www.stunnel.org/faq/certs.html <http://www.stunnel.org/faq/certs.html>`_.
After the command has finished, you should have a usable stunnel.pem in
your working directory.

Next is to create a configuration file for stunnel. It will direct
stunnel what to do. You can use the following basic file:

::

; Certificate/key is needed in server modecert = /etc/stunnel/stunnel.pem; Some debugging stuff useful for troubleshootingdebug = 7foreground=yes

        [ssyslog]
        accept  = 60514
        connect = 61514

Save this file to e.g. /etc/stunnel/syslog-server.conf. Please note that
the settings in *italics* are for debugging only. They run stunnel with
a lot of debug information in the foreground. This is very valuable
while you setup the system - and very useless once everything works
well. So be sure to remove these lines when going to production.

Finally, you need to start the stunnel daemon. Under Debian, this is
done via "stunnel /etc/stunnel/syslog.server.conf". If you have enabled
the debug settings, you will immediately see a lot of nice messages.

Now you have stunnel running, but it obviously unable to talk to rsyslog
- because it is not yet running. If not already done, configure it so
that it does everything you want. If in doubt, you can simply copy
/etc/syslog.conf to /etc/rsyslog.conf and you probably have what you
want. The really important thing in rsyslogd configuration is that you
must make it listen to tcp port 61514 (remember: this is where stunnel
send the messages to). Thankfully, this is easy to achieve: just add "-t
61514" to the rsyslogd startup options in your system startup script.
After done so, start (or restart) rsyslogd.

The server should now be fully operational.

Client Setup
~~~~~~~~~~~~

The client setup is simpler. Most importantly, you do not need a
certificate (of course, you can use one if you would like to
authenticate the client, but this is beyond the scope of this paper). So
the basic thing you need to do is create the stunnel configuration file.

    ::

        ; Some debugging stuff useful for troubleshootingdebug = 7foreground=yes

        client=yes

        [ssyslog]
        accept  = 127.0.0.1:61514
        connect = 192.0.2.1:60514

Again, the text in *italics* is for debugging purposes only. I suggest
you leave it in during your initial testing and then remove it. The most
important difference to the server configuration outlined above is the
"client=yes" directive. It is what makes this stunnel behave like a
client. The accept directive binds stunnel only to the local host, so
that it is protected from receiving messages from the network (somebody
might fake to be the local sender). The address "192.0.2.1" is the
address of the server machine. You must change it to match your
configuration. Save this file to /etc/stunnel/syslog-client.conf.

Then, start stunnel via "stunnel4 /etc/stunnel/syslog-client.conf".  Now
you should see some startup messages. If no errors appear, you have a
running client stunnel instance.

Finally, you need to tell rsyslogd to send data to the remote host. In
stock syslogd, you do this via the "@host" forwarding directive. The
same works with rsyslog, but it supports extensions to use tcp. Add the
following line to your /etc/rsyslog.conf:

    ::

        *.*      @@127.0.0.1:61514



Please note the double at-sign (@@). This is no typo. It tells rsyslog
to use tcp instead of udp delivery. In this sample, all messages are
forwarded to the remote host. Obviously, you may want to limit this via
the usual rsyslog.conf settings (if in doubt, use man rsyslog.con).

You do not need to add any special startup settings to rsyslog on the
client. Start or restart rsyslog so that the new configuration setting
takes place.

Done
~~~~

After following these steps, you should have a working secure syslog
forwarding system. To verify, you can type "logger test" or a similar
smart command on the client. It should show up in the respective server
log file. If you dig out you sniffer, you should see that the traffic on
the wire is actually protected. In the configuration use above, the two
stunnel endpoints should be quite chatty, so that you can follow the
action going on on your system.

If you have only basic security needs, you can probably just remove the
debug settings and take the rest of the configuration to production. If
you are security-sensitive, you should have a look at the various stunnel
settings that help you further secure the system.

Preventing Systems from talking directly to the rsyslog Server
--------------------------------------------------------------

It is possible that remote systems (or attackers) talk to the rsyslog
server by directly connecting to its port 61514. Currently (July of
2005), rsyslog does not offer the ability to bind to the local host,
only. This feature is planned, but as long as it is missing, rsyslog
must be protected via a firewall. This can easily be done via e.g
iptables. Just be sure not to forget it.

Conclusion
----------

With minimal effort, you can set up a secure logging infrastructure
employing ssl encrypted syslog message transmission. As a side note, you
also have the benefit of reliable tcp delivery which is far less prone
to message loss than udp.
