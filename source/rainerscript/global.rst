global() configuration object
=============================

The global configuration object permits to set global parameters. Note
that each parameter can only be set once and cannot be re-set
thereafter. If a parameter is set multiple times, the behaviour is
unpredictable. As with other configuration objects, parameters for this
object are case-insensitive.

The following parameters can be set:

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

  This tells rsyslog if it shall process internal messages itself. The
  default mode of operations ("off") makes rsyslog send messages to the
  system log sink (and if it is the only instance, receive them back from there).
  This also works with systemd journal and will make rsyslog messages show up in the
  systemd status control information.

  If this (instance) of rsyslog is not the main instance and there is another
  main logging system, rsyslog internal messages will be inserted into
  the main instance's syslog stream. In this case, setting to ("on") will
  let you receive the internal messages in the instance they originate from.

  Note that earlier versions of rsyslog worked the opposite way. More
  information about the change can be found in `rsyslog-error-reporting-improved <http://www.rsyslog.com/rsyslog-error-reporting-improved>`_.



- **stdlog.channelspec**

  Permits to set the liblogging-stdlog channel specifier string. This
  in turn permits to send rsyslog log messages to a destination different
  from the system default. Note that this parameter has only effect if
  *processInternalMessages* is set to "off". Otherwise it is silently
  ignored.

- **shutdown.enable.ctlc**

  If set to "on", rsyslogd can be terminated by pressing ctl-c. This is
  most useful for containers. If set to "off" (the default), this is not
  possible.

- **defaultNetstreamDriver**

  Set it to "ossl" or "gtls" to enable TLS.
  This `guide <http://www.rsyslog.com/doc/rsyslog_secure_tls.html>`_
  showes how to use TLS.

- **maxMessageSize**

  Configures the maximum message size allowed for all inputs. Default is 8K.
  Anything above the maximum size will be truncated.

  Note: some modules provide separate parameters that allow overriding this
  setting (e.g., :doc:`imrelp's MaxDataSize parameter <../../configuration/modules/imrelp>`).

.. _global_janitorInterval:

- **janitor.interval** [minutes], available since 8.3.3

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

- **parser.permitSlashInProgramName** [on/off] available in 8.25.0+

  **Default:** off

  This controls whether slashes in the "programname" property
  (the static part of the tag) are permitted or not. By default
  this is not permitted, but some Linux tools (including most
  importantly the journal) store slashes as part of the program
  name inside the syslogtag. In those cases, the ``programname``
  is truncated at the first slash.

  In other words, if the setting is off, a value of ``app/foo[1234]``
  in the tag will result in a programname of ``app``, and if an
  application stores an absolute path name like ``/app/foo[1234]``,
  the ``programname`` property will be empty ("").
  If set to ``on``, a syslogtag of ``/app/foo[1234]`` will result
  in a ``programname`` value of ``/app/foo`` and a syslogtag of
  ``app/foo[1234]`` will result in a ``programname`` value of
  ``app/foo``.

- **senders.keepTrack** [on/off] available 8.17.0+

  **Default:** off

  If turned on, rsyslog keeps track of known senders and also reports
  statistical data for them via the impstats mechanism.

  A list of active senders is kept. When a new sender is detected, an
  informational message is emitted. Senders are purged from the list
  only after a timeout (see *senders.timoutAfter* parameter). Note
  that we do not intentionally remove a sender when a connection is
  closed. The whole point of this sender-tracking is to have the ability
  to provide longer-duration data. As such, we would not like to drop
  information just because the sender has disconnected for a short period
  of time (e.g. for a reboot).

  Senders are tracked by their hostname (taken at connection establishment).

  Note: currently only imptcp and imtcp support sender tracking.

- **senders.timeoutAfter** [seconds] available 8.17.0+

  **Default:** 12 hours (12*60*60 seconds)

  Specifies after which period a sender is considered to "have gone
  away". For each sender, rsyslog keeps track of the time it least
  received messages from it. When it has not received a message during
  that interval, rsyslog considers the sender to be no longer present.
  It will then a) emit a warning message (if configured) and b) purge
  it from the active senders list. As such, the sender will no longer
  be reported in impstats data once it has timed out.

- **senders.reportGoneAway** [on/off] available 8.17.0+

  **Default:** off

  Emit a warning message when now data has been received from a sender
  within the *senders.timeoutAfter* interval.

- **senders.reportNew** [on/off] available 8.17.0+

  **Default:** off

  If sender tracking is active, report a sender that is not yet inside
  the cache. Note that this means that senders which have been timed out
  due to prolonged inactivity are also reported once they connect again.

- **debug.unloadModules** [on/off] available 8.17.0+

  **Default:** on

  This is primarily a debug setting. If set to "off", rsyslog will never
  unload any modules (including plugins). This usually causes no operational
  problems, but may in extreme cases. The core benefit of this setting is
  that it makes valgrind stack traces readable. In previous versions, the
  same functionality was only available via a special build option.

- **debug.files** [ARRAY of filenames] available 8.29.0+

  **Default:** none

  This can be used to configure rsyslog to only show debug-output generated in
  certain files. If the option is set, but no filename is given, the
  debug-output will behave as if the option is turned off.

  Do note however that due to the way the configuration works, this might not
  effect the first few debug-outputs, while rsyslog is reading in the configuration.
  For optimal results we recommend to put this parameter at the very start of
  your configuration to minmize unwanted output.

  See debug.whitelist for more information.

- **debug.whitelist** [on/off] available 8.29.0+

  **Default:** on

  This parameter is an assisting parameter of  debug.files. If debug.files
  is used in the configuration, debug.whitelist is a switch for the files named
  to be either white- or blacklisted from displaying debug-output. If it is set to
  on, the listed files will generate debug-output, but no other files will.
  The reverse principle applies if the parameter is set to off.

  See debug.files for more information.

- **environment** [ARRAY of environment variable=value strings] available 8.23.0+

  **Default:** none

  This permits to set environment variables via rsyslog.conf. The prime
  motivation for having this is that for many libraries, defaults can be
  set via environment variables, **but** setting them via operating system
  service startup files is cumbersome and different on different platforms.
  So the *environment* parameter provides a handy way to set those
  variables.

  A common example is to set the *http_proxy* variable, e.g. for use with
  KSI signing or ElasticSearch. This can be done as follows::

    global(environment="http_proxy=http://myproxy.example.net")

  Note that an environment variable set this way must contain an equal sign,
  and the variable name must not be longer than 127 characters.

  It is possible to set multiple environment variables in a single
  global statement. This is done in regular array syntax as follows::

    global(environment=["http_proxy=http://myproxy.example.net",
                        "another_one=this string is=ok!"
          )

  As usual, whitespace is irrelevant in regard to parameter placing. So
  the above sample could also have been written on a single line.

- **internalmsg.ratelimit.interval** [positive integer] available 8.29.0+

  **Default:** 5

   Specifies the interval in seconds onto which rate-limiting is to be
   applied to internal messgaes generated by rsyslog(i.e. error messages).
   If more than internalmsg.ratelimit.burst messages are read during
   that interval, further messages up to the end of the interval are
   discarded.

- **internalmsg.ratelimit.burst** [positive integer] available 8.29.0+

  **Default:** 500

   Specifies the maximum number of internal messages that can be emitted within
   the ratelimit.interval interval. For futher information, see
   description there.


  **Caution:** Environment variables are set immediately when the
  corresponding statement is encountered. Likewise, modules are loaded when
  the module load statement is encountered. This may create **sequence
  dependencies** inside rsyslog.conf. To avoid this, it is highly suggested
  that environment variables are set **right at the top of rsyslog.conf**.
  Also, rsyslog-related environment variables may not apply even when set
  right at the top. It is safest to still set them in operating system
  start files. Note that rsyslog environment variables are usually intended
  only for developers so there should hardly be a need to set them for a
  regular user. Also, many settings (e.g. debug) are also available as
  configuration objects.

- **errorMessagesToStderr.maxNumber** [positive integer] available 8.30.0+

  **Default:** unlimited

  This permits to put a hard limit on the number of messages that can
  go to stderr. If for nothing else, this capability is helpful for the
  testbench. It permits to reduce spamming the test log while still
  providing the ability to see initial error messages. Might also be
  useful for some practical deployments.

- **variables.caseSensitve** [boolean (on/off)] available 8.30.0+

  **Default:** off

  This permits to make variables case-sensitive, what might be required
  for some exotic input data where case is the only difference in
  field names. Note that in rsyslog versions prior to 8.30, the default was
  "on", which very often led to user confusion. There normally should be no
  need to switch it back to "on", except for the case to be mentioned.
  This is also the reason why we switched the default.

- **dynafile.donotsuspend** [boolean (on/off)] available 8.32.0+

  **Default:** on

  This permits SUSPENDing dynafile actions. Traditionally, SUSPEND mode was
  never entered for dynafiles as it would have blocked overall processing
  flow. Default is not to suspend (and thus block).

- **internal.developeronly.options**

  This is NOT to be used by end users. It provides rsyslog developers the
  ability to do some (possibly strange) things inside rsyslog, e.g. for
  testing. This parameter should never be set, except if instructed by
  a developer. If it is set, rsyslog may misbehave, segfault, or cause
  other strange things. Note that option values are not guaranteed to
  stay the same between releases, so do not be "smart" and apply settings
  that you found via a web search.

  Once again: **users must NOT set this parameter!**

- **oversizemsg.errorfile** [file name] available 8.35.0+

  This parameter is used to specify the name of the oversize message log file.
  Here messages that are longer than maxMessageSize will be gathered.

- **oversizemsg.input.mode** [mode] available 8.35.0+

  With this parameter the behavior for oversized messages can be specified.
  Available modes are:

  - truncate: Oversized messages will be truncated.
  - split: Oversized messages will be split and the rest of the message will
    be send in another message.
  - accept: Oversized messages will still be accepted.

- **oversizemsg.report** [boolean (on/off)] available 8.35.0+

  This parameter specifies if an error shall be reported when an oversized
  message is seen. The default is "on".

- **abortOnUncleanConfig** [boolean (on/of)] available 8.37.0+

  This parameter permits to prevent rsyslog from running when the
  configuration file is not clean. "Not Clean" means there are errors or
  some other annoyances that rsyslgod reports on startup. This is a
  user-requested feature to have a strict startup mode. Note that with the
  current code base it is not always possible to differentiate between an
  real error and a warning-like condition. As such, the startup will also
  prevented if warnings are present. I consider this a good thing in being
  "strict", but I admit there also currently is no other way of doing it.

- **inputs.timeout.shutdown** [numeric, ms] available 8.37.0+

  This parameter specifies how long input modules are given time to terminate
  when rsyslog is shutdown. The default is 1000ms (1 second). If the input
  requires longer to terminate, it will be cancelled. This is necessary if
  the input is inside a lengthy operation, but should generally be tried to
  avoid. On busy systems it may make sense to increase that timeout. This
  especially seems to be the case with containers.
