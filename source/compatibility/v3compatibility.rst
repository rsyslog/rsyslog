Compatibility Notes for rsyslog v3
==================================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net/>`_
*(2008-03-28)*

Rsyslog aims to be a drop-in replacement for sysklogd. However, version
3 has some considerable enhancements, which lead to some backward
compatibility issues both in regard to sysklogd and rsyslog v1 and v2.
Most of these issues are avoided by default by not specifying the -c
option on the rsyslog command line. That will enable
backwards-compatibility mode. However, please note that things may be
suboptimal in backward compatibility mode, so the advise is to work
through this document, update your rsyslog.conf, remove the no longer
supported startup options and then add -c3 as the first option to the
rsyslog command line. That will enable native mode.

Please note that rsyslogd helps you during that process by logging
appropriate messages about compatibility mode and
backwards-compatibility statements automatically generated. You may
want your syslogd log for those. They immediately follow rsyslogd's
startup message.

Inputs
------

With v2 and below, inputs were automatically started together with
rsyslog. In v3, inputs are optional! They come in the form of plug-in
modules. **At least one input module must be loaded to make rsyslog do
any useful work.** The config file directives doc briefly lists which
config statements are available by which modules.

It is suggested that input modules be loaded in the top part of the
config file. Here is an example, also highlighting the most important
modules:

::

  $ModLoad immark # provides --MARK-- message capability
  $ModLoad imudp # provides UDP syslog reception
  $ModLoad imtcp # provides TCP syslog reception
  $ModLoad imgssapi # provides GSSAPI syslog reception
  $ModLoad imuxsock # provides support for local system logging (e.g. via logger command)
  $ModLoad imklog # provides kernel logging support (previously done by rklogd)

Command Line Options
--------------------

A number of command line options have been removed. New config file
directives have been added for them. The -h and -e option have been
removed even in compatibility mode. They are ignored but an informative
message is logged. Please note that -h was never supported in v2, but
was silently ignored. It disappeared some time ago in the final v1
builds. It can be replaced by applying proper filtering inside
syslog.conf.

-c option / Compatibility Mode
------------------------------

The -c option is new and tells rsyslogd about the desired backward
compatibility mode. It must always be the first option on the command
line, as it influences processing of the other options. To use the
rsyslog v3 native interface, specify -c3. To use compatibility mode ,
either do not use -c at all or use -c<vers> where vers is the rsyslog
version that it shall be compatible to. Use -c0 to be command-line
compatible to sysklogd.

Please note that rsyslogd issues warning messages if the -c3 command
line option is not given. This is to alert you that your are running in
compatibility mode. Compatibility mode interferes with your rsyslog.conf
commands and may cause some undesired side-effects. It is meant to be
used with a plain old rsyslog.conf - if you use new features, things
become messy. So the best advise is to work through this document,
convert your options and config file and then use rsyslog in native
mode. In order to aid you in this process, rsyslog logs every
compatibility-mode config file directive it has generated. So you can
simply copy them from your logfile and paste them to the config.

-e Option
---------

This option is no longer supported, as the "last message repeated n
times" feature is now turned off by default. We changed this default
because this feature is causing a lot of trouble and we need to make it
either go away or change the way it works. For more information, please
see our dedicated `forum thread on "last message repeated n
times" <http://www.rsyslog.com/PNphpBB2-viewtopic-p-1130.phtml>`_. This
thread also contains information on how to configure rsyslogd so that it
continues to support this feature (as long as it is not totally
removed).

-m Option
---------

The -m command line option is emulated in compatibility mode. To replace
it, use the following config directives (compatibility mode
auto-generates them):

::

  $ModLoad immark
  $MarkMessagePeriod 1800 # 30 minutes

-r Option
---------

Is no longer available in native mode. However, it is understood in
compatibility mode (if no -c option is given). Use the ``$UDPSeverRun
<port>`` config file directives. You can now also set the local address
the server should listen to via ``$UDPServerAddress <ip>`` config
directive.

The following example configures an UDP syslog server at the local
address 192.0.2.1 on port 514:

::

  $ModLoad imudp
  $UDPServerAddress 192.0.2.1 # this MUST be before the $UDPServerRun directive!
  $UDPServerRun 514

``$UDPServerAddress \*`` means listen on all local interfaces. This is the
default if no directive is specified.

Please note that now multiple listeners are supported. For example, you
can do the following:

::

  $ModLoad imudp
  $UDPServerAddress 192.0.2.1 # this MUST be before the $UDPServerRun directive!
  $UDPServerRun 514
  $UDPServerAddress \* # all local interfaces
  $UDPServerRun 1514

These config file settings run two listeners: one at 192.0.2.1:514 and
one on port 1514, which listens on all local interfaces.

Default port for UDP (and TCP) Servers
--------------------------------------

Please note that with pre-v3 rsyslogd, a service database lookup was
made when a UDP server was started and no port was configured. Only if
that failed, the IANA default of 514 was used. For TCP servers, this
lookup was never done and 514 always used if no specific port was
configured. For consistency, both TCP and UDP now use port 514 as
default. If a lookup is desired, you need to specify it in the "Run"
directive, e.g. "*$UDPServerRun syslog*\ ".

klogd
-----

klogd has (finally) been replaced by a loadable input module. To enable
klogd functionality, do

::

  $ModLoad imklog

Note that this can not be handled by the compatibility layer, as klogd
was a separate binary. A limited set of klogd command line settings is
now supported via rsyslog.conf. That set of configuration directives is
to be expanded. 

Output File Syncing
-------------------

Rsyslogd tries to keep as compatible to stock syslogd as possible. As
such, it retained stock syslogd's default of syncing every file write if
not specified otherwise (by placing a dash in front of the output file
name). While this was a useful feature in past days where hardware was
much less reliable and UPS seldom, this no longer is useful in today's
world. Instead, the syncing is a high performance hit. With it, rsyslogd
writes files around 50 **times** slower than without it. It also affects
overall system performance due to the high IO activity. In rsyslog v3,
syncing has been turned off by default. This is done via a specific
configuration directive

::
  $ActionFileEnableSync on/off

which is off by
default. So even if rsyslogd finds sync selector lines, it ignores them
by default. In order to enable file syncing, the administrator must
specify ``$ActionFileEnableSync on`` at the top of rsyslog.conf. This
ensures that syncing only happens in some installations where the
administrator actually wanted that (performance-intense) feature. In the
fast majority of cases (if not all), this dramatically increases
rsyslogd performance without any negative effects.

Output File Format
------------------

Rsyslog supports high precision RFC 3339 timestamps and puts these into
local log files by default. This is a departure from previous syslogd
behaviour. We decided to sacrifice some backward-compatibility in an
effort to provide a better logging solution. Rsyslog has been supporting
the high-precision timestamps for over three years as of this writing,
but nobody used them because they were not default (one may also assume
that most people didn't even know about them). Now, we are writing the
great high-precision time stamps, which greatly aid in getting the right
sequence of logging events. If you do not like that, you can easily turn
them off by placing

::

  $ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat

right at the start of your rsyslog.conf. This will use the previous
format. Please note that the name is case-sensitive and must be
specified exactly as shown above. Please also note that you can of
course use any other format of your liking. To do so, simply specify the
template to use or set a new default template via the
$ActionFileDefaultTemplate directive. Keep in mind, though, that
templates must be defined before they are used.

Keep in mind that when receiving messages from remote hosts, the
timestamp is just as precise as the remote host provided it. In most
cases, this means you will only a receive a standard timestamp with
second precision. If rsyslog is running at the remote end, you can
configure it to provide high-precision timestamps (see below).

Forwarding Format
-----------------

When forwarding messages to remote syslog servers, rsyslogd by default
uses the plain old syslog format with second-level resolution inside the
timestamps. We could have made it emit high precision timestamps.
However, that would have broken almost all receivers, including earlier
versions of rsyslog. To avoid this hassle, high-precision timestamps
need to be explicitly enabled. To make this as painless as possible,
rsyslog comes with a canned template that contains everything necessary.
 To enable high-precision timestamps, just use:

::

  $ActionForwardDefaultTemplate RSYSLOG_ForwardFormat # for plain TCP and UDP
  $ActionGSSForwardDefaultTemplate RSYSLOG_ForwardFormat # for GSS-API

And, of course, you can always set different forwarding formats by just
specifying the right template.

If you are running in a system with only rsyslog 3.12.5 and above in the
receiver roles, it is suggested to add one (or both) of the above
statements to the top of your rsyslog.conf (but after the $ModLoad's!) -
that will enable you to use the best in timestamp support available.
Please note that when you use this format with other receivers, they
will probably become pretty confused and not detect the timestamp at
all. In earlier rsyslog versions, for example, that leads to duplication
of timestamp and hostname fields and disables the detection of the
original hostname in a relayed/NATed environment. So use the new format
with care.

Queue Modes for the Main Message Queue
--------------------------------------

Either "FixedArray" or "LinkedList" is recommended. "Direct" is
available, but should not be used except for a very good reason
("Direct" disables queueing and will potentially lead to message loss on
the input side).
