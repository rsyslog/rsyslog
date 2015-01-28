global() configuration object
=============================

The global configuration object permits to set global parameters. Note
that each parameter can only be set once and cannot be re-set
thereafter. If a parameter is set multiple times, the behaviour is
unpredictable.

The following paramters can be set:

-  **action.reportSuspension** - binary, default "on", v7.5.8+

   If enabled ("on") action will log message under *syslog.\** when an
   action suspends or resumes itself. This usually happens when there are
   problems connecting to backend systems. If disabled ("off"), these
   messages are not generated. These messages can be useful in detecting
   problems with backend systems. Most importantly, frequent suspension
   and resumption points to a problem area.

- **action.reportSuspensionContinuation** - binary, default "off", v7.6.1+, v8.2.0+

  If enabled ("on") the action will not only report the first suspension but
  each time the suspension is prolonged. Otherwise, the follow-up messages
  are not logged. If this setting is set to "on", action.reportSuspension is
  also automaticaly turned "on".

- **workDirectory**
- **dropMsgsWithMaliciousDNSPtrRecords**
- **localHostname**
- **preserveFQDN**
- **defaultNetstreamDriverCAFile**

  For `TLS syslog <http://www.rsyslog.com/doc/rsyslog_secure_tls.html>`_,
  the CA certificate that can verify the machine keys and certs (see below)

- **defaultNetstreamDriverKeyFile**

  Machine private key

- **defaultNetstreamDriverCertFile**

  Machine public key (certificate)

- **debug.gnutls** (0-10; default:0)

  Any other parameter than 0 enables the debug messages of GnuTLS. the
  amount of messages given depends on the height of the parameter, 0
  being nothing and 10 being very much. Caution! higher parameters may
  give out way more information than needed. We advise you to first use
  small parameters to prevent that from happening.
  **This parameter only has an effect if general debugging is enabled.**

- **processInternalMessages** binary (on/off)

  This tell rsyslog if it shall process internal messages itself. This is
  the default mode of operations ("on") and usually the best. However, if
  this (instance) of rsyslog is not the main instance and there is another
  main logging system, rsyslog internal messages will not be inserted into
  the main instance's syslog stream. To do this, set the parameter to "off",
  in which case rsyslog will send messages to the system log sink (and if
  it is the only instance, receive them back from there). This also works
  with systemd journal and will make rsyslog messages show up in the
  systemd status control information. 

- **stdlog.channelspec**

  Permits to set the liblogging-stdlog channel specifier string. This
  in turn permits to send rsyslog log messages to a destination different
  from the system default. Note that this parameter has only effect if
  *processInternalMessages* is set to "off". Otherwise it is silently
  ignored.


- **defaultNetstreamDriver**

  Set it to "gtls" to enable TLS for `TLS syslog <http://www.rsyslog.com/doc/rsyslog_secure_tls.html>`_

- **maxMessageSize**

  The maximum message size rsyslog can process. Default is 4K. Anything
  above the maximum size will be truncated.

.. _global_janitorInterval:

- **janitorInterval** [minutes], available since 8.3.3

  Sets the interval at which the
  :doc:`janitor process <../concepts/janitor>`
  runs.

- **debug.onShutdown** available in 7.5.8+

  If enabled ("on"), rsyslog will log debug messages when a system
  shutdown is requested. This can be used to track issues that happen
  only during shutdown. During normal operations, system performance is
  NOT affected.
  Note that for this option to be useful, the debug.logFile parameter
  must also be set (or the respective environment variable).

- **debug.logFile** available in 7.5.8+

  This is used to specify the debug log file name. It is used for all
  debug output. Please note that the RSYSLOG\_DEBUGLOG environment
  variable always **overrides** the value of debug.logFile.

- **net.ipprotocol** available in 8.6.0+

  This permits to instruct rsyslog to use IPv4 or IPv6 only. Possible
  values are "unspecified", in which case both protocols are used,
  "ipv4-only", and "ipv6-only", which restrict usage to the specified
  protocol. The default is "unspecified".

  Note: this replaces the former *-4* and *-6* rsyslogd command line
  options.

- **net.aclAddHostnameOnFail** available in 8.6.0+

  If "on", during ACL processing, hostnames are resolved to IP addresses for
  performance reasons. If DNS fails during that process, the hostname
  is added as wildcard text, which results in proper, but somewhat
  slower operation once DNS is up again.
  
  The default is "off".

- **net.aclResolveHostname** available in 8.6.0+
  
  If "off", do not resolve hostnames to IP addresses during ACL processing.
  
  The default is "on".

- **net.enableDNS** [on/off] available in 8.6.0+

  **Default:** on

  Can be used to turn DNS name resolution on or off.
  
- **net.permitACLWarning** [on/off] available in 8.6.0+

  **Default:** on

  If "off", suppress warnings issued when messages are received
  from non-authorized machines (those, that are in no AllowedSender list).

- **parser.parseHostnameAndTag** [on/off] available in 8.6.0+

  **Default:** on

  This controls wheter the parsers try to parse HOSTNAME and TAG fields
  from messages. The default is "on", in which case parsing occurs. If
  set to "off", the fields are not parsed. Note that this usually is
  **not** what you want to have.

  It is highly suggested to change this setting to "off" only if you
  know exactly why you are doing this.
