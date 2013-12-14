`back to rsyslog module overview <rsyslog_conf_modules.html>`_

UDP Syslog Input Module
=======================

**Module Name:    imudp**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Multi-Ruleset Support:**\ since 5.3.2

**Description**:

Provides the ability to receive syslog messages via UDP.

Multiple receivers may be configured by specifying multiple input
actions.

**Configuration Parameters**:

**Module Parameters**:

-  **TimeRequery** <nbr-of-times>
    this is a performance optimization. Getting the system time is very
   costly. With this setting, imudp can be instructed to obtain the
   precise time only once every n-times. This logic is only activated if
   messages come in at a very fast rate, so doing less frequent time
   calls should usually be acceptable. The default value is two, because
   we have seen that even without optimization the kernel often returns
   twice the identical time. You can set this value as high as you like,
   but do so at your own risk. The higher the value, the less precise
   the timestamp.
-  **SchedulingPolicy** <rr/fifo/other>
    Can be used the set the scheduler priority, if the necessary
   functionality is provided by the platform. Most useful to select
   "fifo" for real-time processing under Linux (and thus reduce chance
   of packet loss).
-  **SchedulingPriority** <number>
    Scheduling priority to use.

**Input Parameters**:

-  **Address** <IP>
    local IP address (or name) the UDP listens should bind to
-  **Port** <port>
    default 514, start UDP server on this port. Either a single port can
   be specified or an array of ports. If multiple ports are specified, a
   listener will be automatically started for each port. Thus, no
   additional inputs need to be configured.
   Single port: Port="514"
   Array of ports: Port=["514","515","10514","..."]
-  **Ruleset** <ruleset>
    Binds the listener to a specific `ruleset <multi_ruleset.html>`_.
-  **RateLimit.Interval** [number] - (available since 7.3.1) specifies
   the rate-limiting interval in seconds. Default value is 0, which
   turns off rate limiting. Set it to a number of seconds (5
   recommended) to activate rate-limiting.
-  **RateLimit.Burst** [number] - (available since 7.3.1) specifies the
   rate-limiting burst in number of messages. Default is 10,000.
-  **InputName** [name] - (available since 7.3.9) specifies the value of
   the inputname. In older versions, this was always "imudp" for all
   listeners, which still i the default. Starting with 7.3.9 it can be
   set to different values for each listener. Note that when a single
   input statement defines multipe listner ports, the inputname will be
   the same for all of them. If you want to differentiate in that case,
   use "InputName.AppendPort" to make them unique. Note that the
   "InputName" parameter can be an empty string. In that case, the
   corresponding inputname property will obviously also be the empty
   string. This is primarily meant to be used togehter with
   "InputName.AppendPort" to set the inputname equal to the port.
-  **InputName.AppendPort** [on/**off**] - (available since 7.3.9)
   appends the port the the inputname. Note that when no inputname is
   specified, the default of "imudp" is used and the port is appended to
   that default. So, for example, a listner port of 514 in that case
   will lead to an inputname of "imudp514". The ability to append a port
   is most useful when multiple ports are defined for a single input and
   each of the inputnames shall be unique. Note that there currently is
   no differentiation between IPv4/v6 listeners on the same port.

**Caveats/Known Bugs:**

-  Scheduling parameters are set **after** privileges have been dropped.
   In most cases, this means that setting them will not be possible
   after privilege drop. This may be worked around by using a
   sufficiently-privileged user account.

**Samples:**

This sets up an UPD server on port 514:

module(load="imudp") # needs to be done just once input(type="imudp"
port="514")

In the next example, we set up three listeners at ports 10514, 10515 and
10516 and assign a listner name of "udp" to it, followed by the port
number:

module(load="imudp") input(type="imudp" port=["10514","10515","10516"]
inputname="udp" inputname.appendPort="on")

The next example is almost equal to the previous one, but now the
inputname property will just be set to the port number. So if a message
was received on port 10515, the input name will be "10515" in this
example whereas it was "udp10515" in the previous one. Note that to do
that we set the inputname to the empty string.

module(load="imudp") input(type="imudp" port=["10514","10515","10516"]
inputname="" inputname.appendPort="on")

**Legacy Configuration Directives**:

Multiple receivers may be configured by specifying $UDPServerRun
multiple times.

-  $UDPServerAddress <IP>
    equivalent to: Address
-  $UDPServerRun <port>
    equivalent to: Port
-  $UDPServerTimeRequery <nbr-of-times>
    equivalent to: TimeRequery
-  $InputUDPServerBindRuleset <ruleset>
    equivalent to: Ruleset
-  $IMUDPSchedulingPolicy <rr/fifo/other> Available since 4.7.4+,
   5.7.3+, 6.1.3+.
    equivalent to: SchedulingPolicy
-  $IMUDPSchedulingPriority <number> Available since 4.7.4+, 5.7.3+,
   6.1.3+.
    equivalent to: SchedulingPriority

**Sample:**

This sets up an UPD server on port 514:

$ModLoad imudp # needs to be done just once $UDPServerRun 514

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2009-2013 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
