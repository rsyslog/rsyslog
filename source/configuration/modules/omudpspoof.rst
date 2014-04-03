`rsyslog module reference <rsyslog_conf_modules.html>`_

UDP spoofing output module (omudpspoof)
=======================================

**Module Name:    omstdout**

**Author:**\ David Lang <david@lang.hm> and Rainer Gerhards
<rgerhards@adiscon.com>

**Available Since**: 5.1.3

**Description**:

This module is similar to the regular UDP forwarder, but permits to
spoof the sender address. Also, it enables to circle through a number of
source ports.

**Configuration Directives**:

-  **$ActionOMOMUDPSpoofSourceNameTemplate** <templatename>
    This is the name of the template that contains a numerical IP
   address that is to be used as the source system IP address. While it
   may often be a constant value, it can be generated as usual via the
   property replacer, as long as it is a valid IPv4 address. If not
   specified, the build-in default template
   RSYSLOG\_omudpspoofDfltSourceTpl is used. This template is defined as
   follows:
    $template RSYSLOG\_omudpspoofDfltSourceTpl,"%fromhost-ip%"
    So in essence, the default template spoofs the address of the system
   the message was received from. This is considered the most important
   use case.
-  **$ActionOMUDPSpoofTargetHost** <hostname>
    Host that the messages shall be sent to.
-  **$ActionOMUDPSpoofTargetPort** <port>
    Remote port that the messages shall be sent to.
-  **$ActionOMUDPSpoofDefaultTemplate** <templatename>
    This setting instructs omudpspoof to use a template different from
   the default template for all of its actions that do not have a
   template specified explicitely.
-  **$ActionOMUDPSpoofSourcePortStart** <number>
    Specifies the start value for circeling the source ports. Must be
   less than or equal to the end value. Default is 32000.
-  **$ActionOMUDPSpoofSourcePortEnd** <number>
    Specifies the ending value for circeling the source ports. Must be
   less than or equal to the start value. Default is 42000.

**Caveats/Known Bugs:**

-  **IPv6** is currently not supported. If you need this capability,
   please let us know via the rsyslog mailing list.

**Sample:**

The following sample forwards all syslog messages in standard form to
the remote server server.example.com. The original sender's address is
used. We do not care about the source port. This example is considered
the typical use case for omudpspoof.

$ModLoad omudpspoof $ActionOMUDPSpoofTargetHost server.example.com
\*.\*      :omudpspoof:

The following sample forwards all syslog messages in unmodified form to
the remote server server.example.com. The sender address 192.0.2.1 with
fixed source port 514 is used.

$ModLoad omudpspoof $template spoofaddr,"192.0.2.1" $template
spooftemplate,"%rawmsg%" $ActionOMUDPSpoofSourceNameTemplate spoofaddr
$ActionOMUDPSpoofTargetHost server.example.com
$ActionOMUDPSpoofSourcePortStart 514 $ActionOMUDPSpoofSourcePortEnd 514
\*.\*      :omudpspoof:;spooftemplate

The following sample is similar to the previous, but uses as many
defaults as possible. In that sample, a source port in the range
32000..42000 is used. The message is formatted according to rsyslog's
canned default forwarding format. Note that if any parameters have been
changed, the previously set defaults will be used!

$ModLoad omudpspoof $template spoofaddr,"192.0.2.1"
$ActionOMUDPSpoofSourceNameTemplate spoofaddr
$ActionOMUDPSpoofTargetHost server.example.com \*.\*      :omudpspoof:

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2009-2012 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
