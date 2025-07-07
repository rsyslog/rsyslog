Preserving syslog sender over NAT
=================================

Question:
I have a number of syslog clients behind a NAT device. The receiver receives syslog messages that travelled over the NAT device. This leads the receiver to believe that all messages originated from the same IP address. With stock syslogd, I can not differentiate between the senders. Is there any way to record the correct sender of the message with rsyslog?

Answer:
OK, I’ve now had some real lab time. The good news in short: if you use rsyslog both on the senders as well as on the receiver, you do NOT have any problems with NAT.

To double-check (and out of curiosity), I also tried with stock syslogd. I used the ones that came with RedHat and FreeBSD. Neither of them reports the sending machine correctly, they all report the NAT address. Obviously, this is what made this thread appear, but it is a good verification for the correctness of my lab. Next, I tried rsyslogd on the sender and stock syslogd on the receiver (just RedHat this time). The machine was still incorrectly displayed as the NAT address. However, now the real machine name immediately followed the NAT address, so you could differentiate the different machines – but in a inconsistent way.

Finally, I tried to run the stock syslogds against rsyslogd. Again, the host was not properly displayed. Actually, this time the host was not displayed at all (with the default rsyslogd template). Instead, the tag showed up in the host field. So this configuration is basically unusable.

The root cause of the NAT issue with stock syslogd obviously is that it does NOT include the HOST header that should be sent as of RFC 3164. This requires the receiver to take the host from the socket, which – in a NATed environment – can only hold the mangled NAT address. Rsyslog instead includes the HOST header, so the actual host name can be taken from that (this is the way rsyslog works with the default templates).

I barely remember seeing this in code when I initially forked rsyslog from sysklogd. I have not verified it once again. I have also not tested with syslog-ng, simply because that is not my prime focus and a lab would have required too much time.

To make a long story short: If you use rsyslog on both the senders and receivers, NAT is no issue for you.