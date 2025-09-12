global() configuration object
=============================

The global configuration object permits to set global parameters. Note
that each parameter can only be set once and cannot be re-set
thereafter. If a parameter is set multiple times, the behaviour is
unpredictable. As with other configuration objects, parameters for this
object are case-insensitive.

The following parameters can be set:

-  **action.reportSuspension** - binary, default "on", v7.5.8+

   If enabled ("on") action will log message under `*syslog.\**` when an
   action suspends or resumes itself. This usually happens when there are
   problems connecting to backend systems. If disabled ("off"), these
   messages are not generated. These messages can be useful in detecting
   problems with backend systems. Most importantly, frequent suspension
   and resumption points to a problem area.

- **action.reportSuspensionContinuation** - binary, default "off", v7.6.1+, v8.2.0+

  If enabled ("on") the action will not only report the first suspension but
  each time the suspension is prolonged. Otherwise, the follow-up messages
  are not logged. If this setting is set to "on", action.reportSuspension is
  also automatically turned "on".

- **workDirectory**

  Sets the directory that rsyslog uses for work files, e.g. imfile state
  or queue spool files.

- **umask** available 8.26.0+

  Sets the rsyslogd process' umask.  If not specified, the system-provided default
  is used.  The value given must always be a 4-digit octal number, with the initial
  digit being zero.

- **dropMsgsWithMaliciousDNSPtrRecords**

- **localHostname**
  Permits to overwrite the local host hostname.

- **preserveFQDN**

  Keep fully qualified hostnames instead of stripping the local domain.
  The default is "off" for sysklogd compatibility. Reverse lookup results
  are cached; see :ref:`reverse_dns_cache` for options to refresh cached
  names.
- **defaultNetstreamDriverCAFile**

  For `TLS syslog <http://www.rsyslog.com/doc/rsyslog_secure_tls.html>`_,
  the CA certificate that can verify the machine keys and certs (see below)

- **defaultNetstreamDriverCRLFile**

  For `TLS syslog <http://www.rsyslog.com/doc/rsyslog_secure_tls.html>`_,
  the CRL File contains a List contains a list of revoked certificates.

- **defaultNetstreamDriverKeyFile**

  Machine private key

- **defaultNetstreamDriverCertFile**

  Machine public key (certificate)

- **debug.gnutls** (0-10; default:0)

  Any other parameter than 0 enables the debug messages of GnuTLS. The
  amount of messages given depends on the height of the parameter, 0
  being nothing and 10 being very much. Caution! higher parameters may
  give out way more information than needed. We advise you to first use
  small parameters to prevent that from happening.
  **This parameter only has an effect if general debugging is enabled.**

- **netstreamDriverCaExtraFiles**

  This directive allows to configure multiple additional extra CA files.
  This is intended for SSL certificate chains to work appropriately,
  as the different CA files in the chain need to be specified.
  It must be remarked that this parameter only works with the OpenSSL driver.

- **defaultopensslengine** available 8.2406.0+

  This parameter is used to specify a custom OpenSSL engine by its ID. If the
  engine is not specified, the system will use the default engine provided by OpenSSL.

  Note: Listing Available OpenSSL Engines
  To determine the available OpenSSL engines on your system, use the following command:

  .. code-block:: bash

      openssl engine -t

  This command will output a list of available engines along with their IDs and descriptions.
  Use the engine ID from this list as the value for the defaultopensslengine parameter.

  .. code-block:: text

      (rdrand) Intel RDRAND engine
      (dynamic) Dynamic engine loading support

  Example configuration:

  .. code-block:: text

      global(
        ...
        defaultopensslengine="rdrand"
        ...
      )

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
  shows how to use TLS.

- **maxMessageSize**

  Configures the maximum message size allowed for all inputs. Default is 8K.
  Anything above the maximum size will be truncated.

  Note: some modules provide separate parameters that allow overriding this
  setting (e.g., :doc:`imrelp's MaxDataSize parameter <../../configuration/modules/imrelp>`).

.. _global_janitorInterval:

- **janitor.interval** [minutes], available 8.3.3+

  Sets the interval at which the
  :doc:`janitor process <../concepts/janitor>`
  runs.

- **debug.onShutdown** available 7.5.8+

  If enabled ("on"), rsyslog will log debug messages when a system
  shutdown is requested. This can be used to track issues that happen
  only during shutdown. During normal operations, system performance is
  NOT affected.
  Note that for this option to be useful, the debug.logFile parameter
  must also be set (or the respective environment variable).

- **debug.logFile** available 7.5.8+

  This is used to specify the debug log file name. It is used for all
  debug output. Please note that the RSYSLOG\_DEBUGLOG environment
  variable always **overrides** the value of debug.logFile.

- **net.ipprotocol** available 8.6.0+

  This permits to instruct rsyslog to use IPv4 or IPv6 only. Possible
  values are "unspecified", in which case both protocols are used,
  "ipv4-only", and "ipv6-only", which restrict usage to the specified
  protocol. The default is "unspecified".

  Note: this replaces the former *-4* and *-6* rsyslogd command line
  options.

- **net.aclAddHostnameOnFail** available 8.6.0+

  If "on", during ACL processing, hostnames are resolved to IP addresses for
  performance reasons. If DNS fails during that process, the hostname
  is added as wildcard text, which results in proper, but somewhat
  slower operation once DNS is up again.

  The default is "off".

- **net.aclResolveHostname** available 8.6.0+

  If "off", do not resolve hostnames to IP addresses during ACL processing.

  The default is "on".

- **net.enableDNS** [on/off] available 8.6.0+

  **Default:** on

  Can be used to turn DNS name resolution on or off. When disabled, no
  reverse lookups are performed and the cache described in
  :ref:`reverse_dns_cache` is bypassed.

- **net.permitACLWarning** [on/off] available 8.6.0+

  **Default:** on

  If "off", suppress warnings issued when messages are received
  from non-authorized machines (those, that are in no AllowedSender list).

- **parser.parseHostnameAndTag** [on/off] available 8.6.0+

  **Default:** on

  This controls whether the parsers try to parse HOSTNAME and TAG fields
  from messages. The default is "on", in which case parsing occurs. If
  set to "off", the fields are not parsed. Note that this usually is
  **not** what you want to have.

  It is highly suggested to change this setting to "off" only if you
  know exactly why you are doing this.

- **parser.permitSlashInProgramName** [on/off] available 8.25.0+

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

- **parser.escapeControlCharacterTab** [on/off] available since 8.7.0

  **Default:** on

  If set to "off", the TAB control character (US-ASCII HT) will not be
  escaped. If set to "on", it will be escaped to the sequence "#011".
  Note that escaping is the traditional behavior and existing scripts
  may get into trouble if this is changed to "off".

- **parser.controlCharacterEscapePrefix** [char]

  **Default:** '#'

  This option specifies the prefix character to be used for control
  character escaping (see option
  *parser.escapeControlCharactersOnReceive*).

- **parser.escape8BitCharactersOnReceive** [on/off]

  **Default:** off

  This parameter instructs rsyslogd to replace non US-ASCII characters
  (those that have the 8th bit set) during reception of the message.
  This may be useful for some systems. Please note that this escaping
  breaks Unicode and many other encodings. Most importantly, it can be
  assumed that Asian and European characters will be rendered hardly
  readable by this settings. However, it may still be useful when the
  logs themselves are primarily in English and only occasionally contain
  local script. If this option is turned on, all control-characters are
  converted to a 3-digit octal number and be prefixed with the
  *parser.controlCharacterEscapePrefix* character (being '#' by default).

  **Warning:**

  -  turning on this option most probably destroys non-western character
     sets (like Japanese, Chinese and Korean) as well as European
     character sets.
  -  turning on this option destroys digital signatures if such exists
     inside the message
  -  if turned on, the drop-cc, space-cc and escape-cc `property
     replacer <property_replacer.html>`_ options do not work as expected
     because control characters are already removed upon message
     reception. If you intend to use these property replacer options, you
     must turn off *parser.escape8BitCharactersOnReceive*.

- **parser.escapeControlCharactersOnReceive** [on/off]

  **Default:** on

  This parameter instructs rsyslogd to replace control characters during
  reception of the message. The intent is to provide a way to stop
  non-printable messages from entering the syslog system as whole. If this
  option is turned on, all control-characters are converted to a 3-digit
  octal number and be prefixed with the *parser.controlCharacterEscapePrefix*
  character (being '#' by default). For example, if the BEL character
  (ctrl-g) is included in the message, it would be converted to '#007'.
  To be compatible to sysklogd, this option must be turned on.

  **Warning:**

  -  turning on this option most probably destroys non-western character
     sets (like Japanese, Chinese and Korean)
  -  turning on this option destroys digital signatures if such exists
     inside the message
  -  if turned on, the drop-cc, space-cc and escape-cc `property
     replacer <property_replacer.html>`_ options do not work as expected
     because control characters are already removed upon message
     reception. If you intend to use these property replacer options, you
     must turn off *parser.escapeControlCharactersOnReceive*.


- **senders.keepTrack** [on/off] available 8.17.0+

  **Default:** off

  If turned on, rsyslog keeps track of known senders and also reports
  statistical data for them via the impstats mechanism.

  A list of active senders is kept. When a new sender is detected, an
  informational message is emitted. Senders are purged from the list
  only after a timeout (see *senders.timeoutAfter* parameter). Note
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
  your configuration to minimize unwanted output.

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
                        "another_one=this string is=ok!"]
          )

  As usual, whitespace is irrelevant in regard to parameter placing. So
  the above sample could also have been written on a single line.

- **internalmsg.ratelimit.interval** [positive integer] available 8.29.0+

  **Default:** 5

   Specifies the interval in seconds onto which rate-limiting is to be
   applied to internal messages generated by rsyslog(i.e. error messages).
   If more than internalmsg.ratelimit.burst messages are read during
   that interval, further messages up to the end of the interval are
   discarded.

- **internalmsg.ratelimit.burst** [positive integer] available 8.29.0+

  **Default:** 500

   Specifies the maximum number of internal messages that can be emitted within
   the ratelimit.interval interval. For further information, see
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

- **internalmsg.severity** [syslog severity value] available 8.1905.0+

  **Default:** info

  This permits to limit which internal messages are emitted by rsyslog. This
  is especially useful if internal messages are reported to systemd journal,
  which is the default on journal systems. In that case there is no other
  ability to filter out messages before they are logged by the journal.

  While any syslog severity value can be used, the most useful ones are

  * `error`, to see only error messages but ignore anything else
  * `warn`, to also see warning messages (highly recommended)
  * `info`, to also see informational messages like events generated
     by DA queues status checks. This is the default as the informational
     messages often provide valuable information.
  * `debug`, to see all messages, including only those interesting for
     debugging. While this is still considerably lower volume than a
     rsyslog developer debug log, this can be quite verbose. Selecting
     `debug` without hard need thus is **not** recommended.

  We expect that users are most often interested in limiting verboseness
  to warning messages. This can be done e.g. via::

    global(internalmsg.severity="warn")

- **errorMessagesToStderr.maxNumber** [positive integer] available 8.30.0+

  **Default:** unlimited

  This permits to put a hard limit on the number of messages that can
  go to stderr. If for nothing else, this capability is helpful for the
  testbench. It permits to reduce spamming the test log while still
  providing the ability to see initial error messages. Might also be
  useful for some practical deployments.

- **variables.caseSensitive** [boolean (on/off)] available 8.30.0+

  **Default:** off

  This permits to make variables case-sensitive, what might be required
  for some exotic input data where case is the only difference in
  field names. Note that in rsyslog versions prior to 8.30, the default was
  "on", which very often led to user confusion. There normally should be no
  need to switch it back to "on", except for the case to be mentioned.
  This is also the reason why we switched the default.

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
    be sent in another message.
  - accept: Oversized messages will still be accepted.

- **oversizemsg.report** [boolean (on/off)] available 8.35.0+

  This parameter specifies if an error shall be reported when an oversized
  message is seen. The default is "on".

- **abortOnUncleanConfig** [boolean (on/off)] available 8.37.0+

  This parameter permits to prevent rsyslog from running when the
  configuration file is not clean. "Not Clean" means there are errors or
  some other annoyances that rsyslogd reports on startup. This is a
  user-requested feature to have a strict startup mode. Note that with the
  current code base it is not always possible to differentiate between a
  real error and a warning-like condition. As such, the startup will also
  prevented if warnings are present. I consider this a good thing in being
  "strict", but I admit there also currently is no other way of doing it.

- **abortOnFailedQueueStartup** [boolean (on/off)] available 8.2210.0+

  This parameter is similar to *abortOnUncleanConfig* but makes rsyslog
  abort when there are any problems with queue startup. This is usually
  caused by disk queue settings or disk queue file corruption. Normally,
  rsyslog ignores disk queue definitions in this case and switches the
  queue to emergency mode, which permits in-memory operations. This is
  desired by the fast majority of users, because it permits rsyslog to
  remain operational and process all remaining actions as well as handle
  actions associated with the failed queue decently.
  When this setting is "on", rsyslog aborts immediately when a queue
  problem is detected during startup. If you use this mode, ensure that
  your startup scripts monitor for these type of errors and handle them
  appropriately.
  In our opinion, it is much safer to let rsyslog start and monitor queue
  error messages.

  The **default** for this setting is "off"

- **inputs.timeout.shutdown** [numeric, ms] available 8.37.0+

  This parameter specifies how long input modules are given time to terminate
  when rsyslog is shutdown. The default is 1000ms (1 second). If the input
  requires longer to terminate, it will be cancelled. This is necessary if
  the input is inside a lengthy operation, but should generally be tried to
  avoid. On busy systems it may make sense to increase that timeout. This
  especially seems to be the case with containers.

- **default.action.queue.timeoutshutdown** [numeric] available 8.1901.0+
- **default.action.queue.timeoutactioncompletion** [numeric] available 8.1901.0+
- **default.action.queue.timeoutenqueue** [numeric] available 8.1901.0+
- **default.action.queue.timeoutworkerthreadshutdown** [numeric] available 8.1901.0+

  These parameters set global queue defaults for the respective queue settings.

.. _reverse_dns_cache:

Reverse DNS caching
-------------------

.. index::
   single: reverse DNS cache
   single: dnsCacheTTL
   single: dnscacheEnableTTL
   single: dnscacheDefaultTTL
   single: reverse DNS refresh
   single: DNS lookup cache timeout

Rsyslog caches results from reverse DNS lookups. When TTL expiry is disabled
(*reverselookup.cache.ttl.enable*="off", the default), cache entries remain
valid for the lifetime of the process. Enabling the TTL mechanism permits
automatic refresh after a fixed interval. These controls apply **only** to
reverse lookups of inbound messages. Forward lookups for outbound connections
(for example, resolving `server.example.net` for *omfwd*) use the system
resolver each time and are not cached by rsyslog, so no TTL setting exists for
them.

- **reverselookup.cache.ttl.enable** [boolean (on/off)] available 8.1904.0+

  Controls whether cached hostnames expire. If set to "on", entries expire
  after the duration specified by *reverselookup.cache.ttl.default*. When set
  to "off" the cache never expires.

- **reverselookup.cache.ttl.default** [numeric, seconds] available 8.1904.0+

  Fixed time-to-live for cached reverse lookup results. The **default** value
  is 24 hours. Setting this parameter to ``0`` effectively disables caching,
  which can severely degrade performance, especially for UDP inputs.

These settings interact with ``preserveFQDN`` and ``net.enableDNS``. If DNS
resolution is disabled globally, no caching occurs.

Example:

.. code-block:: none

   global(
     reverselookup.cache.ttl.enable="on"
     reverselookup.cache.ttl.default="3600"    # seconds
   )

Historically these options were referenced in source code and change logs as
``dnscacheEnableTTL`` and ``dnscacheDefaultTTL``.

- **security.abortOnIDResolutionFail** [boolean (on/off)], default "on", available 8.2002.0+

  This setting controls if rsyslog should error-terminate when a security ID cannot
  be resolved during config file processing at startup. If set to "on" and
  a name ID lookup fails (for user and group names) rsyslog does not start but
  terminate with an error message. This is necessary as a security
  measure, as otherwise the wrong permissions can be assigned or privileges
  are not dropped. This setting is applied wherever security IDs are resolved,
  e.g. when dropping privileges or assigning file permissions or owners.

  The setting should be at the top of the configuration parameters to make sure its
  behavior is correctly applied on all other configuration parameters.

  **CHANGE OF BEHAVIOR**

  The default for this parameter is "on". In versions prior to 8.2002.0, the default
  was "off" (by virtue of this parameter not existing). As such, existing
  configurations may now error out.

  We have decided to accept this change of behavior because of the potential
  security implications.

- **operatingStateFile** [string, filename], default unset, available 8.39.0+

  The operatingStateFile, as the name says, provides information about rsyslog
  operating state. It can be useful for troubleshooting.

  If this parameter is not set, an operating state file will not be written. If
  it is set, the file will be written **and** used to detect unclean shutdown.
  Upon startup, rsyslog checks if the last recorded line contains the "clean
  shutdown notification". If so, the file is deleted and re-written with new
  operating state. If the notification cannot be found, rsyslog assumes unclean
  shutdown and complains about this state. In this case the operating state file
  is renamed to "<configured-name>.previous" and a new file is started under the
  configured name for the current run. This permits the administrator to check the
  previous operating state file for helpful information on why the system shut
  down unclean.

- **reportChildProcessExits** [none|errors|all], default "errors", available
  8.1901.0+

  Tells rsyslog whether and when to log a message (under *syslog.\**) when a
  child process terminates. The available modes are:

  - none: Do not report any child process termination.
  - errors: Only report the termination of child processes that have exited with
    a non-zero exit code, or that have been terminated by a signal.
  - all: Report all child process terminations.

  The logged message will be one of the following:
  
  - "program 'x' (pid n) exited with status s" (with "info" severity if the
    status is zero, and "warning" severity otherwise)
  - "program 'x' (pid n) terminated by signal s" (with "warning" severity)

  In some cases, the program name is not included in the message (but only the PID).

  Normally, if a child process terminates prematurely for some reason, rsyslog will
  also report some specific error message the next time it interacts with the process
  (for example, in the case of a process started by omprog, if omprog cannot send a
  message to the process because the pipe is broken, it will report an error
  indicating this). This specific error message (if any) is not affected by this
  global setting.


- **default.ruleset.queue.timeoutshutdown**
- **default.ruleset.queue.timeoutactioncompletion**
- **default.ruleset.queue.timeoutenqueue**
- **default.ruleset.queue.timeoutworkerthreadshutdown**

  Sets default parameters for ruleset queues. See queue doc for the meaning of
  the individual settings.


- **default.action.queue.timeoutshutdown**
- **default.action.queue.timeoutactioncompletion**
- **default.action.queue.timeoutenqueue**
- **default.action.queue.timeoutworkerthreadshutdown**

  Sets default parameters for action queues. See queue doc for the meaning of
  the individual settings.


- **shutdown.queue.doublesize**

  This setting (default "off") permits to temporarily increase the maximum queue
  size during shutdown processing. This is useful when rsyslog needs to re-enqueue
  some messages at shutdown *and* the queue is already full. Note that the need to
  re-enqueue messages stems back to some failed operations. Note that the maximum
  permitted queue size is doubled, as this ensures in all cases that re-enqueuing
  can be completed. Note also that the increase of the max size is temporary during
  shutdown and also does not require any more storage. Except, of course, for
  re-enqueued message.

  The situation addressed by this setting is unlikely to happen, but it could happen.
  To enable the functionality, set it to "on".

- **parser.supportCompressionExtension** [boolean (on/off)] available 8.2106.0+

  This parameter permits to disable rsyslog's single-message-compression extension on
  reception ("off"). The default is to keep it activated ("on").

  The single-message-compression extension permits senders to zip-compress single
  syslog messages. Such messages start with the letter "z" instead of the usual
  syslog PRI value. For well-formed syslog messages, the extension works as designed.
  However, some users transport non-syslog data via rsyslog, and such messages may
  validly start with "z" for non-compressed data. To support such non-standard
  cases, this option can be used to globally disable support for compression on
  all inputs.

privdrop.group.name
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "", "no", "``$PrivDropToGroup``"

.. versionadded:: 8.2110.0

Name of the group rsyslog should run under after startup. Please
note that this group is looked up in the system tables. If the lookup
fails, privileges are NOT dropped. Thus it is advisable to use the
less convenient `privdrop.group.id` parameter. Note that all
supplementary groups are removed by default from the process if the
`privdrop.group.keepsupplemental` parameter is not specified.
If the group id can be looked up, but can not be set,
rsyslog aborts.

Note: See the :doc:`privilege drop documentation<../configuration/droppriv>`
for more details on dropping privileges on startup.

privdrop.group.id
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "", "no", "``$PrivDropToGroupID``"

.. versionadded:: 8.2110.0

Numerical user ID of the group rsyslog should run under after startup.
This is more reliable than the `privdrop.group.name` parameter, which
relies on presence of the group name in system tables. The change to
the ID will always happen if the ID is valid.

Note: See the :doc:`privilege drop documentation<../configuration/droppriv>`
for more details on dropping privileges on startup.

privdrop.user.name
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "", "no", "``$PrivDropToUser``"

.. versionadded:: 8.2110.0


Name of the user rsyslog should run under after startup. Please note
that this user is looked up in the system tables. If the lookup
fails, privileges are NOT dropped. Thus it is advisable to use the
less convenient `privdrop.user.id` parameter. If the user id can be
looked up, but can not be set, rsyslog aborts.

Note: See the :doc:`privilege drop documentation<../configuration/droppriv>`
for more details on dropping privileges on startup.

privdrop.user.id
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "", "no", "``$PrivDropToUserID``"

.. versionadded:: 8.2110.0

Numerical user ID of the user rsyslog should run under after startup.
This is more reliable than the `privdrop.user.name` parameter, which
relies on presence of the user name in system tables. The change to
the ID will always happen if the ID is valid.

Note: See the :doc:`privilege drop documentation<../configuration/droppriv>`
for more details on dropping privileges on startup.

libcapng.default
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "on", "no", "none"

.. versionadded:: 8.2306.0

The `libcapng.default` global option defines how rsyslog should behave
in case something went wrong when capabilities were to be dropped.
The default value is "on", in which case rsyslog exits on a libcapng
related error. If set to "off", an error message describing the problem
appears at startup, nothing more. Default value is preserved for backwards
compatibility.

libcapng.enable
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "on", "no", "none"

.. versionadded:: 8.2310.0

The `libcapng.enable` global option defines whether rsyslog should
drop capabilities at startup or not. By default, it is set to "on".
Until this point, if the project was compiled with --enable-libcap-ng option,
capabilities were automatically dropped. This is configurable now.

compactjsonstring
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.2510.0

The `compactjsonstring` global option defines whether JSON strings generated
by rsyslog should be in the most compact form, even without spaces.
The traditional default is that spaces are introduced. This increases
readability for humans, but needs more ressources (disk, transfer, computation)
in automatted pipelines.
To keep things as compatible as possible, we leave the default as "off" but
recommend that this option is turned on for use in data pipelines.
