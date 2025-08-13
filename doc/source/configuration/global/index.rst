Legacy Global Configuration Statements
======================================
Global configuration statements, as their name implies, usually affect
some global features. However, some also affect main queues, which
are "global" to a ruleset.

True Global Directives
----------------------

.. toctree::
   :glob:

   options/rsconf1_abortonuncleanconfig
   options/rsconf1_debugprintcfsyslinehandlerlist
   options/rsconf1_debugprintmodulelist
   options/rsconf1_debugprinttemplatelist
   options/rsconf1_failonchownfailure
   options/rsconf1_generateconfiggraph
   options/rsconf1_includeconfig
   options/rsconf1_mainmsgqueuesize
   options/rsconf1_maxopenfiles
   options/rsconf1_moddir
   options/rsconf1_modload
   options/rsconf1_umask
   options/rsconf1_resetconfigvariables

-  **$MaxMessageSize** <size\_nbr>, default 8k - allows to specify
   maximum supported message size (both for sending and receiving). The
   default should be sufficient for almost all cases. Do not set this
   below 1k, as it would cause interoperability problems with other
   syslog implementations.

   **Important:** In order for this directive to work correctly,
   it **must** be placed right at the top of ``rsyslog.conf``
   (before any input is defined).

   Change the setting to e.g. 32768 if you would like to support large
   message sizes for IHE (32k is the current maximum needed for IHE). I
   was initially tempted to set the default to 32k, but there is a some
   memory footprint with the current implementation in rsyslog.
   If you intend to receive Windows Event Log data (e.g. via
   `EventReporter <http://www.eventreporter.com/>`_), you might want to
   increase this number to an even higher value, as event log messages
   can be very lengthy ("$MaxMessageSize 64k" is not a bad idea). Note:
   testing showed that 4k seems to be the typical maximum for **UDP**
   based syslog. This is an IP stack restriction. Not always ... but
   very often. If you go beyond that value, be sure to test that
   rsyslogd actually does what you think it should do ;) It is highly
   suggested to use a TCP based transport instead of UDP (plain TCP
   syslog, RELP). This resolves the UDP stack size restrictions.
   Note that 2k, is the smallest size that must be
   supported in order to be compliant to the upcoming new syslog RFC
   series.
-  **$LocalHostName** [name] - this directive permits to overwrite the
   system hostname with the one specified in the directive. If the
   directive is given multiple times, all but the last one will be
   ignored. Please note that startup error messages may be issued with
   the real hostname. This is by design and not a bug (but one may argue
   if the design should be changed ;)). Available since 4.7.4+, 5.7.3+,
   6.1.3+.
-  **$LogRSyslogStatusMessages** [**on**/off] - If set to on (the
   default), rsyslog emits message on startup and shutdown as well as
   when it is HUPed. This information might be needed by some log
   analyzers. If set to off, no such status messages are logged, what
   may be useful for other scenarios. [available since 4.7.0 and 5.3.0]
-  **$DefaultRuleset** [name] - changes the default ruleset for unbound
   inputs to the provided *name* (the default ruleset is named
   "RSYSLOG\_DefaultRuleset"). It is advised to also read our paper on
   :doc:`using multiple rule sets in rsyslog <../../concepts/multi_ruleset>`.
- **$DefaultNetstreamDriver** <drivername>, the default
  :doc:`network stream driver <../../concepts/netstrm_drvr>` to use.
  Defaults to ptcp.
-  **$DefaultNetstreamDriverCAFile** </path/to/cafile.pem>
-  **$DefaultNetstreamDriverCertFile** </path/to/certfile.pem>
-  **$DefaultNetstreamDriverKeyFile** </path/to/keyfile.pem>
-  **$NetstreamDriverCaExtraFiles** </path/to/extracafile.pem> - This
   directive allows to configure multiple additional extra CA files.
   This is intended for SSL certificate chains to work appropriately,
   as the different CA files in the chain need to be specified.
   It must be remarked that this directive only works with the OpenSSL driver.
-  **$RepeatedMsgContainsOriginalMsg** [on/**off**] - "last message
   repeated n times" messages, if generated, have a different format
   that contains the message that is being repeated. Note that only the
   first "n" characters are included, with n to be at least 80
   characters, most probably more (this may change from version to
   version, thus no specific limit is given). The bottom line is that n
   is large enough to get a good idea which message was repeated but it
   is not necessarily large enough for the whole message. (Introduced
   with 4.1.5). Once set, it affects all following actions.
-  **$OptimizeForUniprocessor** - This directive is no longer supported.
   While present in versions prior to 8.32.0, the directive had no effect
   for many years. Attempts to use the directive now results in a warning.
-  **$PreserveFQDN** [on/**off**) - if set to off (legacy default to remain
   compatible to sysklogd), the domain part from a name that is within
   the same domain as the receiving system is stripped. If set to on,
   full names are always used. Reverse lookup results are cached; see
   :ref:`reverse_dns_cache` for controlling cache refresh.
-  **$WorkDirectory** <name> (directory for spool and other work files. Do
   **not** use trailing slashes)
-  `$PrivDropToGroup <droppriv.html>`_
-  `$PrivDropToGroupID <droppriv.html>`_
-  `$PrivDropToUser <droppriv.html>`_
-  `$PrivDropToUserID <droppriv.html>`_
-  **$Sleep** <seconds> - puts the rsyslog main thread to sleep for the
   specified number of seconds immediately when the directive is
   encountered. You should have a good reason for using this directive!
-  **$LocalHostIPIF** <interface name> - (available since 5.9.6) - if
   provided, the IP of the specified interface (e.g. "eth0") shall be
   used as fromhost-ip for local-originating messages. If this
   directive is not given OR the interface cannot be found (or has no IP
   address), the default of "127.0.0.1" is used. Note that this
   directive can be given only once. Trying to reset will result in an
   error message and the new value will be ignored. Please note that
   modules must have support for obtaining the local IP address set via
   this directive. While this is the case for rsyslog-provided modules,
   it may not always be the case for contributed plugins.
   **Important:** This directive shall be placed **right at the top of
   rsyslog.conf**. Otherwise, if error messages are triggered before
   this directive is processed, rsyslog will fix the local host IP to
   "127.0.0.1", what than can not be reset.
-  **$ErrorMessagesToStderr** [**on**\ \|off] - direct rsyslogd error
   message to stderr (in addition to other targets)
-  **$SpaceLFOnReceive** [on/**off**] - instructs rsyslogd to replace LF
   with spaces during message reception (sysklogd compatibility aid).
   This is applied at the beginning of the parser stage and cannot
   be overridden (neither at the input nor parser level). Consequently,
   it affects all inputs and parsers.

main queue specific Directives
------------------------------
Note that these directives modify ruleset main message queues.
This includes the default ruleset's main message queue, the one
that is always present. To do this, specify directives outside of
a ruleset definition.

To understand queue parameters, read
:doc:`queues in rsyslog <../../concepts/queues>`.

-  **$MainMsgQueueCheckpointInterval** <number>
-  **$MainMsgQueueDequeueBatchSize** <number> [default 32]
-  **$MainMsgQueueDequeueSlowdown** <number> [number is timeout in
   *micro*\ seconds (1000000us is 1sec!), default 0 (no delay). Simple
   rate-limiting!]
-  **$MainMsgQueueDiscardMark** <number> [default 98% of queue size]
-  **$MainMsgQueueDiscardSeverity** <severity> [either a textual or
   numerical severity! default 4 (warning)]
-  **$MainMsgQueueFileName** <name>
-  **$MainMsgQueueHighWaterMark** <number> [default 90% of queue size]
-  **$MainMsgQueueImmediateShutdown** [on/**off**]
-  **$MainMsgQueueLowWaterMark** <number> [default 70% of queue size]
-  **$MainMsgQueueMaxFileSize** <size\_nbr>, default 1m
-  **$MainMsgQueueTimeoutActionCompletion** <number> [number is timeout in
   ms (1000ms is 1sec!), default 1000, 0 means immediate!]
-  **$MainMsgQueueTimeoutEnqueue** <number> [number is timeout in ms (1000ms
   is 1sec!), default 2000, 0 means discard immediately]
-  **$MainMsgQueueTimeoutShutdown** <number> [number is timeout in ms
   (1000ms is 1sec!), default 0 (indefinite)]
-  **$MainMsgQueueWorkerTimeoutThreadShutdown** <number> [number is timeout
   in ms (1000ms is 1sec!), default 60000 (1 minute)]
-  **$MainMsgQueueType** [**FixedArray**/LinkedList/Direct/Disk]
-  **$MainMsgQueueSaveOnShutdown**  [on/**off**]
-  **$MainMsgQueueWorkerThreads** <number>, num worker threads, default 2,
   recommended 1
-  **$MainMsgQueueWorkerThreadMinumumMessages** <number>, default queue size/number of workers
