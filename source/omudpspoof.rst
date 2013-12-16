`rsyslog module reference <rsyslog_conf_modules.html>`_

UDP spoofing output module (omudpspoof)
=======================================

**Module Name:    omstdout**

**Authors:**\ Rainer Gerhards <rgerhards@adiscon.com> and David Lang
<david@lang.hm>

**Available Since**: 5.1.3 / v7 config since 7.2.5

**Description**:

This module is similar to the regular UDP forwarder, but permits to
spoof the sender address. Also, it enables to circle through a number of
source ports.

**Important:** This module requires root priveleges for its low-level
socket access. As such, the **module will not work if rsyslog is
configured to drop privileges**.

**load() Parameters**:

-  **Template**\ [templateName]
    sets a non-standard default template for this module.

 

**action() parameters**:

-  **Target**\ string
    Name or IP-Address of the system that shall receive messages. Any
   resolvable name is fine.
-  **Port**\ [Integer, Default 514]
    Name or numerical value of port to use when connecting to target.
-  **Template**\ [Word]
    Template to use as message text.
-  **SourceTemplate**\ [Word]
    This is the name of the template that contains a numerical IP
   address that is to be used as the source system IP address. While it
   may often be a constant value, it can be generated as usual via the
   property replacer, as long as it is a valid IPv4 address. If not
   specified, the build-in default template
   RSYSLOG\_omudpspoofDfltSourceTpl is used. This template is defined as
   follows:
    template(name="RSYSLOG\_omudpspoofDfltSourceTpl" type="string"
   string="%fromhost-ip%")
    So in essence, the default template spoofs the address of the system
   the message was received from. This is considered the most important
   use case.
-  **SourcePortStart**\ [Word]
    Specifies the start value for circeling the source ports. Must be
   less than or equal to the end value. Default is 32000.
-  **SourcePortEnd**\ [Word]
    Specifies the ending value for circeling the source ports. Must be
   less than or equal to the start value. Default is 42000.
-  **mtu**\ [Integer, default 1500]
    Maximum MTU supported by the network. Default respects Ethernet and
   must usually not be adjusted. Setting a too-high MTU can lead to
   message loss, too low to excess message fragmentation. Change only if
   you really know what you are doing. This is always given in number of
   bytes.

**pre-v7 Configuration Directives**:

-  **$ActionOMOMUDPSpoofSourceNameTemplate** <templatename> - equivalent
   to the "sourceTemplate" parameter.
-  **$ActionOMUDPSpoofTargetHost** <hostname> - equivalent to the
   "target" parameter.
-  **$ActionOMUDPSpoofTargetPort** <port> - equivalent to the "target"
   parameter.
-  **$ActionOMUDPSpoofDefaultTemplate** <templatename> - equivalent to
   the "template" load() parameter.
-  **$ActionOMUDPSpoofSourcePortStart** <number> - equivalent to the
   "SourcePortStart" parameter.
-  **$ActionOMUDPSpoofSourcePortEnd** <number> - equivalent to the
   "SourcePortEnd" parameter.

**Caveats/Known Bugs:**

-  **IPv6** is currently not supported. If you need this capability,
   please let us know via the rsyslog mailing list.
-  Versions shipped prior to rsyslog 7.2.5 do not support message sizes
   over 1472 bytes (more pricesely: over the network-supported MTU).
   Starting with 7.2.5, those messages will be fragmented, up to a total
   upper limit of 64K (induced by UDP). Message sizes over 64K will be
   truncated. For older versions, messages over 1472 may be totally
   discarded or truncated, depending on version and environment.

**Config Samples**

The following sample forwards all syslog messages in standard form to
the remote server server.example.com. The original sender's address is
used. We do not care about the source port. This example is considered
the typical use case for omudpspoof.

module(load="omudpspoof") action(type="omudpspoof"
target="server.example.com")

The following sample forwards all syslog messages in unmodified form to
the remote server server.example.com. The sender address 192.0.2.1 with
fixed source port 514 is used.

module(load="omudpspoof") template(name="spoofaddr" type="string"
string="192.0.2.1") template(name="spooftemplate" type="string"
string="%rawmsg%") action(type="omudpspoof" target="server.example.com"
sourcetemplate="spoofaddr" template="spooftemplate"
sourceport.start="514" sourceport.end="514)

The following sample is exatly like the previous, but it specifies a
larger size MTU. If, for example, the envrionment supports Jumbo
Ethernet frames, increasing the MTU is useful as it reduces packet
fragmentation, which most often is the source of problems. Note that
setting the MTU to a value larger than the local-attached network
supports will lead to send errors and loss of message. So use with care!

module(load="omudpspoof") template(name="spoofaddr" type="string"
string="192.0.2.1") template(name="spooftemplate" type="string"
string="%rawmsg%") action(type="omudpspoof" target="server.example.com"
sourcetemplate="spoofaddr" template="spooftemplate"
sourceport.start="514" sourceport.end="514 mtu="8000")

Of course, the action can be combined with any type of filter, for
example a tradition PRI filter:

module(load="omudpspoof") template(name="spoofaddr" type="string"
string="192.0.2.1") template(name="spooftemplate" type="string"
string="%rawmsg%") local0.\* action(type="omudpspoof"
target="server.example.com" sourcetemplate="spoofaddr"
template="spooftemplate" sourceport.start="514" sourceport.end="514
mtu="8000")

... or any complex expression-based filter:

module(load="omudpspoof") template(name="spoofaddr" type="string"
string="192.0.2.1") template(name="spooftemplate" type="string"
string="%rawmsg%") if prifilt("local0.\*") and $msg contains "error"
then action(type="omudpspoof" target="server.example.com"
sourcetemplate="spoofaddr" template="spooftemplate"
sourceport.start="514" sourceport.end="514 mtu="8000")

and of course it can also be combined with as many other actions as one
likes:

module(load="omudpspoof") template(name="spoofaddr" type="string"
string="192.0.2.1") template(name="spooftemplate" type="string"
string="%rawmsg%") if prifilt("local0.\*") and $msg contains "error"
then { action(type="omudpspoof" target="server.example.com"
sourcetemplate="spoofaddr" template="spooftemplate"
sourceport.start="514" sourceport.end="514 mtu="8000")
action(type="omfile" file="/var/log/somelog") stop # or whatever... }

**Legacy Sample (pre-v7):**

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
