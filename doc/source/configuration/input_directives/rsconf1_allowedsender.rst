$AllowedSender
--------------

**Type:** input configuration parameter

**Default:** all allowed

**Description:**

*Note:* this feature is supported for backward-compatibility, only.
The rsyslog team recommends to use proper firewalling instead of
this feature.

Allowed sender lists can be used to specify which remote systems are
allowed to send syslog messages to rsyslogd. With them, further hurdles
can be placed between an attacker and rsyslogd. If a message from a
system not in the allowed sender list is received, that message is
discarded. A diagnostic message is logged, so that the fact is recorded
(this message can be turned off with the "-w" rsyslogd command line
option).

Allowed sender lists can be defined for UDP and TCP senders separately.
There can be as many allowed senders as needed. The syntax to specify
them is:

::

  $AllowedSender <type>, ip[/bits], ip[/bits]

"$AllowedSender" is the parameter - it must be written exactly as shown
and the $ must start at the first column of the line. "<type>" is either "UDP"
or "TCP" (or "GSS", if this is enabled during compilation).
It must immediately be followed by the comma, else you will
receive an error message. "ip[/bits]" is a machine or network ip address
as in "192.0.2.0/24" or "127.0.0.1". If the "/bits" part is omitted, a
single host is assumed (32 bits or mask 255.255.255.255). "/0" is not
allowed, because that would match any sending system. If you intend to
do that, just remove all $AllowedSender parameters. If more than 32 bits
are requested with IPv4, they are adjusted to 32. For IPv6, the limit is
128 for obvious reasons. Hostnames, with and without wildcards, may also
be provided. If so, the result of revers DNS resolution is used for
filtering. Multiple allowed senders can be specified in a
comma-delimited list. Also, multiple $AllowedSender lines can be given.
They are all combined into one UDP and one TCP list. Performance-wise,
it is good to specify those allowed senders with high traffic volume
before those with lower volume. As soon as a match is found, no further
evaluation is necessary and so you can save CPU cycles.

Rsyslogd handles allowed sender detection very early in the code, nearly
as the first action after receiving a message. This keeps the access to
potential vulnerable code in rsyslog at a minimum. However, it is still
a good idea to impose allowed sender limitations via firewalling.

**WARNING:** by UDP design, rsyslogd can not identify a spoofed sender
address in UDP syslog packets. As such, a malicious person could spoof
the address of an allowed sender, send such packets to rsyslogd and
rsyslogd would accept them as being from the faked sender. To prevent
this, use syslog via TCP exclusively. If you need to use UDP-based
syslog, make sure that you do proper egress and ingress filtering at the
firewall and router level.

Rsyslog also detects some kind of malicious reverse DNS entries. In any
case, using DNS names adds an extra layer of vulnerability. We recommend
to stick with hard-coded IP addresses wherever possible.

**Sample:**

::

  $AllowedSender UDP, 127.0.0.1, 192.0.2.0/24, [::1]/128, *.example.net, somehost.example.com

