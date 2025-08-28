rsyslog Properties
==================

Data items in rsyslog are called "properties". They can have different
origin. The most important ones are those that stem from received
messages. But there are also others. Whenever you want to access data items,
you need to access the respective property.

Properties are used in

- :doc:`templates <templates>`
- conditional statements

The property name is case-insensitive (prior to 3.17.0, they were case-sensitive).

Note: many users refer to "rsyslog properties" as "rsyslog variables". You can treat
them as synonymous.
Read how `rsyslog lead author Rainer Gerhards explains the naming
difference <https://rainer.gerhards.net/2020/08/rsyslog-template-variables-where-to-find-them.html">`_.

Message Properties
------------------
These are extracted by rsyslog parsers from the original message. All message
properties start with a letter.

The following message properties exist:

**msg**
  the MSG part of the message (aka "the message" ;))

**rawmsg**
  the message "as is".  Should be useful for debugging and also if a message
  should be forwarded totally unaltered.
  Please notice *EscapecontrolCharactersOnReceive* is enabled by default, so
  it may be different from what was received in the socket.

**rawmsg-after-pri**
  Almost the same as **rawmsg**, but the syslog PRI is removed.
  If no PRI was present, **rawmsg-after-pri** is identical to
  **rawmsg**. Note that the syslog PRI is header field that
  contains information on syslog facility and severity. It is
  enclosed in greater-than and less-than characters, e.g.
  "<191>". This field is often not written to log files, but
  usually needs to be present for the receiver to properly
  classify the message. There are some rare cases where one
  wants the raw message, but not the PRI. You can use this
  property to obtain that. In general, you should know that you
  need this format, otherwise stay away from the property.

**hostname**
  hostname from the message

**source**
  alias for HOSTNAME

**fromhost**
  hostname of the system the message was received from (in a relay chain,
  this is the system immediately in front of us and not necessarily the
  original sender). This is a DNS-resolved name, except if that is not
  possible or DNS resolution has been disabled. Reverse lookup results are
  cached; see :ref:`reverse_dns_cache` for controlling cache timeout. Forward
  lookups for outbound connections are not cached by rsyslog and are resolved
  via the system resolver whenever a connection is made.

**fromhost-ip**
  The same as fromhost, but always as an IP address. Local inputs (like
  imklog) use 127.0.0.1 in this property.

**syslogtag**
  TAG from the message

**programname**
  the "static" part of the tag, as defined by BSD syslogd. For example,
  when TAG is "named[12345]", programname is "named".

  Precisely, the programname is terminated by either (whichever occurs first):

  - end of tag
  - nonprintable character
  - ':'
  - '['
  - '/'

  The above definition has been taken from the FreeBSD syslogd sources.

  Please note that some applications include slashes in the static part
  of the tag, e.g. "app/foo[1234]". In this case, programname is "app".
  If they store an absolute path name like in "/app/foo[1234]", programname
  will become empty (""). If you need to actually store slashes as
  part of the programname, you can use the global option

  global(parser.permitSlashInProgramName="on")

  to permit this. Then, a syslogtag of "/app/foo[1234]" will result in
  programname being "/app/foo". Note: this option is available starting at
  rsyslogd version 8.25.0.

**pri**
  PRI part of the message - undecoded (single value)

**pri-text**
  the PRI part of the message in a textual form with the numerical PRI
  appended in brackets (e.g. "local0.err<133>")

**iut**
  the monitorware InfoUnitType - used when talking to a
  `MonitorWare <https://www.monitorware.com>`_ backend (also for
  `Adiscon LogAnalyzer <https://loganalyzer.adiscon.com/>`_)

**syslogfacility**
  the facility from the message - in numerical form

**syslogfacility-text**
  the facility from the message - in text form

**syslogseverity**
  severity from the message - in numerical form

**syslogseverity-text**
  severity from the message - in text form

**syslogpriority**
  an alias for syslogseverity - included for historical reasons (be
  careful: it still is the severity, not PRI!)

**syslogpriority-text**
  an alias for syslogseverity-text

**timegenerated**
  timestamp when the message was RECEIVED. Always in high resolution

**timereported**
  timestamp from the message. Resolution depends on what was provided in
  the message (in most cases, only seconds)

**timestamp**
  alias for timereported

**protocol-version**
  The contents of the PROTOCOL-VERSION field from IETF draft
  draft-ietf-syslog-protocol

**structured-data**
  The contents of the STRUCTURED-DATA field from IETF draft
  draft-ietf-syslog-protocol

**app-name**
  The contents of the APP-NAME field from IETF draft
  draft-ietf-syslog-protocol

**procid**
  The contents of the PROCID field from IETF draft
  draft-ietf-syslog-protocol

**msgid**
  The contents of the MSGID field from IETF draft
  draft-ietf-syslog-protocol

**inputname**
  The name of the input module that generated the message (e.g.
  "imuxsock", "imudp"). Note that not all modules necessarily provide this
  property. If not provided, it is an empty string. Also note that the
  input module may provide any value of its liking. Most importantly, it
  is **not** necessarily the module input name. Internal sources can also
  provide inputnames. Currently, "rsyslogd" is defined as inputname for
  messages internally generated by rsyslogd, for example startup and
  shutdown and error messages. This property is considered useful when
  trying to filter messages based on where they originated - e.g. locally
  generated messages ("rsyslogd", "imuxsock", "imklog") should go to a
  different place than messages generated somewhere else.

**uuid**

  *Only Available if rsyslog is build with --enable-uuid*

  A UUID for the message. It is not present by default, but will be created
  on first read of the uuid property. Thereafter, in the local rsyslog
  instance, it will always be the same value. This is also true if rsyslog
  is restarted and messages stayed in an on-disk queue.

  Note well: the UUID is **not** automatically transmitted to remote
  syslog servers when forwarding. If that is needed, a special template
  needs to be created that contains the uuid. Likewise, the receiver must
  parse that UUID from that template.

  The uuid property is most useful if you would like to track a single
  message across multiple local destination. An example is messages being
  written to a database as well as to local files.

**jsonmesg**

  *Available since rsyslog 8.3.0*

  The whole message object as JSON representation. Note that the JSON
  string will *not* include an LF and it will contain *all other message
  properties* specified here as respective JSON containers. It also includes
  all message variables in the "$!" subtree (this may be null if none are
  present).

  This property is primarily meant as an interface to other systems and
  tools that want access to the full property set (namely external
  plugins). Note that it contains the same data items potentially multiple
  times. For example, parts of the syslog tag will by contained in the
  rawmsg, syslogtag, and programname properties. As such, this property
  has some additional overhead. Thus, it is suggested to be used only
  when there is actual need for it.

System Properties
-----------------
These properties are provided by the rsyslog core engine. They are **not**
related to the message. All system properties start with a dollar-sign.

Special care needs to be taken in regard to time-related system variables:

* ``timereported`` contains the timestamp that is contained within the
  message header. Ideally, it resembles the time when the message was
  created at the original sender.
  Depending on how long the message was in the relay chain, this
  can be quite old.
* ``timegenerated`` contains the timestamp when the message was received
  by the local system. Here "received" actually means the point in time
  when the message was handed over from the OS to rsyslog's reception
  buffers, but before any actual processing takes place. This also means
  a message is "received" before it is placed into any queue. Note that
  depending on the input, some minimal processing like extraction of the
  actual message content from the receive buffer can happen. If multiple
  messages are received via the same receive buffer (a common scenario
  for example with TCP-based syslog), they bear the same ``timegenerated``
  stamp because they actually were received at the same time.
* ``$now`` is **not** from the message. It is the system time when the
  message is being **processed**. There is always a small difference
  between ``timegenerated`` and ``$now`` because processing always
  happens after reception. If the message is sitting inside a queue
  on the local system, the time difference between the two can be some
  seconds (e.g. due to a message burst and in-memory queueing) up to
  several hours in extreme cases where a message is sitting inside a
  disk queue (e.g. due to a database outage). The ``timereported``
  property is usually older than ``timegenerated``, but may be totally
  different due to differences in time and time zone configuration
  between systems.

The following system properties exist:

**$bom**
  The UTF-8 encoded Unicode byte-order mask (BOM). This may be useful in
  templates for RFC5424 support, when the character set is known to be
  Unicode.
  
**$myhostname**
  The name of the current host as it knows itself (probably useful for
  filtering in a generic way)

Time-Related System Properties
..............................

All of these system properties exist in a local time variant (e.g. \$now)
and a variant that emits UTC (e.g. \$now-utc). The UTC variant is always
available by appending "-utc". Note that within a single template, only
the localtime or UTC variant should be used. While it is possible to mix
both variants within a single template, it is **not** guaranteed that
they will provide exactly the same time. The technical reason is that
rsyslog needs to re-query system time when the variant is changed. Because
of this, we strongly recommend not mixing both variants in the same
template.

Note that use in different templates will generate a consistent timestamp
within each template. However, as $now always provides local system time
at time of using it, time may advance and consequently different templates
may have different time stamp. To avoid this, use *timegenerated* instead.

**$now**
  The current date stamp in the format YYYY-MM-DD

**$year**
  The current year (4-digit)

**$month**
  The current month (2-digit)

**$day**
  The current day of the month (2-digit)

**$wday**
  The current week day as defined by 'gmtime()'. 0=Sunday, ..., 6=Saturday

**$hour**
  The current hour in military (24 hour) time (2-digit)

**$hhour**
  The current half hour we are in. From minute 0 to 29, this is always 0
  while from 30 to 59 it is always 1.

**$qhour**
  The current quarter hour we are in. Much like $HHOUR, but values range
  from 0 to 3 (for the four quarter hours that are in each hour)

**$minute**
  The current minute (2-digit)

**$now-unixtimestamp**
  The current time as a unix timestamp (seconds since epoch). This actually
  is a monotonically increasing counter and as such can also be used for any
  other use cases that require such counters. This is an example of how
  to use it for rate-limiting::

    # Get Unix timestamp of current message
    set $.tnow = $$now-unixtimestamp

    # Rate limit info to 5 every 60 seconds
    if ($!severity == 6 and $!facility == 17) then {
      if (($.tnow - $/trate) > 60) then {
        # 5 seconds window expired, allow more messages
        set $/trate = $.tnow;
        set $/ratecount = 0;
      }
      if ($/ratecount > 5) then {
        # discard message
        stop
      } else {
        set $/ratecount = $/ratecount + 1;
      }
    }

  NOTE: by definition, there is no "UTC equivalent" of the
  $now-unixtimestamp property.
