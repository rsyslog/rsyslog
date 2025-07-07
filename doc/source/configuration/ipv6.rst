Notes on IPv6 Handling in Rsyslog
=================================

**Rsyslog fully\* supports sending and receiving syslog messages via
both IPv4 and IPv6.** IPv6 is natively supported for both UDP and TCP.
However, there are some options that control handling of IPv6
operations. I thought it is a good idea to elaborate a little about
them, so that you can probably find your way somewhat easier.

First of all, you can restrict rsyslog to using IPv4 or IPv6 addresses
only by specifying the -4 or -6 command line option (now guess which one
does what...). If you do not provide any command line option, rsyslog
uses IPv4 and IPv6 addresses concurrently. In practice, that means the
listener binds to both addresses (provided they are configured). When
sending syslog messages, rsyslog uses IPv4 addresses when the receiver
can be reached via IPv4 and IPv6 addresses if it can be reached via
IPv6. If it can be reached on either IPv4 and v6, rsyslog leaves the
choice to the socket layer. The important point to know is that it uses
whatever connectivity is available to reach the destination.

**There is one subtle difference between UDP and TCP.** With the new
IPv4/v6 ignorant code, rsyslog has potentially different ways to reach
destinations. The socket layer returns all of these paths in a sorted
array. For TCP, rsyslog loops through this array until a successful TCP
connect can be made. If that happens, the other addresses are ignored
and messages are sent via the successfully-connected socket.

For UDP, there is no such definite success indicator. Sure, the socket
layer may detect some errors, but it may not notice other errors (due to
the unreliable nature of UDP). By default, the UDP sender also tries one
entry after the other in the sorted array of destination addresses. When
a send fails, the next address is tried. When the send function finally
succeeds, rsyslogd assumes the UDP packet has reached its final
destination. However, if rsyslogd is started with the "-A" (capital A!)
was given on the command line, rsyslogd will continue to send messages
until the end of the destination address array is reached. This may
result in duplicate messages, but it also provides some additional
reliability in case a message could not be received. You need to be sure
about the implications before applying this option. In general, it is
NOT recommended to use the -A option.

**\***\ rsyslog does not support RFC 3195 over IPv6. The reason is that
the RFC 3195 library, `liblogging <http://www.liblogging.org/>`_,
supports IPv4, only. Currently, there are no plans to update either
rsyslog to another RFC 3195 stack or update liblogging. There is simply
no demand for 3195 solutions.

