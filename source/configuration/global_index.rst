This is a part of the rsyslog.conf documentation.

`back <rsyslog_conf.html>`_

Configuration Directives
------------------------

All configuration directives need to be specified on a line by their own
and must start with a dollar-sign. Note that those starting with the
word "Action" modify the next action and should be specified in front of
it.

Here is a list in alphabetical order. Follow links for a description.

Not all directives have an in-depth description right now. Default
values for them are in bold. A more in-depth description will appear as
implementation progresses.

**Be sure to read information about `queues in rsyslog <queues.html>`_**
- many parameter settings modify queue parameters. If in doubt, use the
default, it is usually well-chosen and applicable in most cases.

-  `$AbortOnUncleanConfig <rsconf1_abortonuncleanconfig.html>`_ - abort
   startup if there is any issue with the config file
-  `$ActionExecOnlyWhenPreviousIsSuspended <rsconf1_actionexeconlywhenpreviousissuspended.html>`_
-  $ActionName <a\_single\_word> - used primarily for documentation,
   e.g. when generating a configuration graph. Available sice 4.3.1.
-  $ActionExecOnlyOnceEveryInterval <seconds> - execute action only if
   the last execute is at last <seconds> seconds in the past (more info
   in `ommail <ommail.html>`_, but may be used with any action)
-  ***$ActionExecOnlyEveryNthTime** <number>* - If configured, the next
   action will only be executed every n-th time. For example, if
   configured to 3, the first two messages that go into the action will
   be dropped, the 3rd will actually cause the action to execute, the
   4th and 5th will be dropped, the 6th executed under the action, ...
   and so on. Note: this setting is automatically re-set when the actual
   action is defined.
-  ***$ActionExecOnlyEveryNthTimeTimeout** <number-of-seconds>* - has a
   meaning only if $ActionExecOnlyEveryNthTime is also configured for
   the same action. If so, the timeout setting specifies after which
   period the counting of "previous actions" expires and a new action
   count is begun. Specify 0 (the default) to disable timeouts.
    *Why is this option needed?* Consider this case: a message comes in
   at, eg., 10am. That's count 1. Then, nothing happens for the next 10
   hours. At 8pm, the next one occurs. That's count 2. Another 5 hours
   later, the next message occurs, bringing the total count to 3. Thus,
   this message now triggers the rule.
    The question is if this is desired behavior? Or should the rule only
   be triggered if the messages occur within an e.g. 20 minute window?
   If the later is the case, you need a
    $ActionExecOnlyEveryNthTimeTimeout 1200
    This directive will timeout previous messages seen if they are older
   than 20 minutes. In the example above, the count would now be always
   1 and consequently no rule would ever be triggered.
-  $ActionFileDefaultTemplate [templateName] - sets a new default
   template for file actions
-  $ActionFileEnableSync [on/off] - enables file syncing capability of
   omfile
-  $ActionForwardDefaultTemplate [templateName] - sets a new default
   template for UDP and plain TCP forwarding action
-  $ActionGSSForwardDefaultTemplate [templateName] - sets a new default
   template for GSS-API forwarding action
-  $ActionQueueCheckpointInterval <number>
-  $ActionQueueDequeueBatchSize <number> [default 16]
-  $ActionQueueDequeueSlowdown <number> [number is timeout in
   *micro*\ seconds (1000000us is 1sec!), default 0 (no delay). Simple
   rate-limiting!]
-  $ActionQueueDiscardMark <number> [default 9750]
-  $ActionQueueDiscardSeverity <number> [\*numerical\* severity! default
   4 (warning)]
-  $ActionQueueFileName <name>
-  $ActionQueueHighWaterMark <number> [default 8000]
-  $ActionQueueImmediateShutdown [on/**off**]
-  $ActionQueueSize <number>
-  $ActionQueueLowWaterMark <number> [default 2000]
-  $ActionQueueMaxFileSize <size\_nbr>, default 1m
-  $ActionQueueTimeoutActionCompletion <number> [number is timeout in ms
   (1000ms is 1sec!), default 1000, 0 means immediate!]
-  $ActionQueueTimeoutEnqueue <number> [number is timeout in ms (1000ms
   is 1sec!), default 2000, 0 means indefinite]
-  $ActionQueueTimeoutShutdown <number> [number is timeout in ms (1000ms
   is 1sec!), default 0 (indefinite)]
-  $ActionQueueWorkerTimeoutThreadShutdown <number> [number is timeout
   in ms (1000ms is 1sec!), default 60000 (1 minute)]
-  $ActionQueueType [FixedArray/LinkedList/**Direct**/Disk]
-  $ActionQueueSaveOnShutdown  [on/**off**]
-  $ActionQueueWorkerThreads <number>, num worker threads, default 1,
   recommended 1
-  $ActionQueueWorkerThreadMinumumMessages <number>, default 100
-  `$ActionResumeInterval <rsconf1_actionresumeinterval.html>`_
-  $ActionResumeRetryCount <number> [default 0, -1 means eternal]
-  $ActionSendResendLastMsgOnReconnect <[on/**off**]> specifies if the
   last message is to be resend when a connecition breaks and has been
   reconnected. May increase reliability, but comes at the risk of
   message duplication.
-  $ActionSendStreamDriver <driver basename> just like
   $DefaultNetstreamDriver, but for the specific action
-  $ActionSendStreamDriverMode <mode>, default 0, mode to use with the
   stream driver (driver-specific)
-  $ActionSendStreamDriverAuthMode <mode>,  authentication mode to use
   with the stream driver. Note that this directive requires TLS
   netstream drivers. For all others, it will be ignored.
   (driver-specific)
-  $ActionSendStreamDriverPermittedPeer <ID>,  accepted fingerprint
   (SHA1) or name of remote peer. Note that this directive requires TLS
   netstream drivers. For all others, it will be ignored.
   (driver-specific) - directive may go away!
-  **$ActionSendTCPRebindInterval** nbr- [available since 4.5.1] -
   instructs the TCP send action to close and re-open the connection to
   the remote host every nbr of messages sent. Zero, the default, means
   that no such processing is done. This directive is useful for use
   with load-balancers. Note that there is some performance overhead
   associated with it, so it is advisable to not too often "rebind" the
   connection (what "too often" actually means depends on your
   configuration, a rule of thumb is that it should be not be much more
   often than once per second).
-  **$ActionSendUDPRebindInterval** nbr- [available since 4.3.2] -
   instructs the UDP send action to rebind the send socket every nbr of
   messages sent. Zero, the default, means that no rebind is done. This
   directive is useful for use with load-balancers.
-  **$ActionWriteAllMarkMessages** [on/**off**]- [available since 5.1.5]
   - normally, mark messages are written to actions only if the action
   was not recently executed (by default, recently means within the past
   20 minutes). If this setting is switched to "on", mark messages are
   always sent to actions, no matter how recently they have been
   executed. In this mode, mark messages can be used as a kind of
   heartbeat. Note that this option auto-resets to "off", so if you
   intend to use it with multiple actions, it must be specified in front
   off **all** selector lines that should provide this functionality.
-  `$AllowedSender <rsconf1_allowedsender.html>`_
-  `$ControlCharacterEscapePrefix <rsconf1_controlcharacterescapeprefix.html>`_
-  `$DebugPrintCFSyslineHandlerList <rsconf1_debugprintcfsyslinehandlerlist.html>`_
-  `$DebugPrintModuleList <rsconf1_debugprintmodulelist.html>`_
-  `$DebugPrintTemplateList <rsconf1_debugprinttemplatelist.html>`_
-  $DefaultNetstreamDriver <drivername>, the default `network stream
   driver <netstream.html>`_ to use. Defaults
   to ptcp.$DefaultNetstreamDriverCAFile </path/to/cafile.pem>
-  $DefaultNetstreamDriverCertFile </path/to/certfile.pem>
-  $DefaultNetstreamDriverKeyFile </path/to/keyfile.pem>
-  **$DefaultRuleset** *name* - changes the default ruleset for unbound
   inputs to the provided *name* (the default default ruleset is named
   "RSYSLOG\_DefaultRuleset"). It is advised to also read our paper on
   `using multiple rule sets in rsyslog <multi_ruleset.html>`_.
-  **$CreateDirs** [**on**/off] - create directories on an as-needed
   basis
-  `$DirCreateMode <rsconf1_dircreatemode.html>`_
-  `$DirGroup <rsconf1_dirgroup.html>`_
-  `$DirOwner <rsconf1_dirowner.html>`_
-  `$DropMsgsWithMaliciousDnsPTRRecords <rsconf1_dropmsgswithmaliciousdnsptrrecords.html>`_
-  `$DropTrailingLFOnReception <rsconf1_droptrailinglfonreception.html>`_
-  `$DynaFileCacheSize <rsconf1_dynafilecachesize.html>`_
-  `$Escape8BitCharactersOnReceive <rsconf1_escape8bitcharsonreceive.html>`_
-  `$EscapeControlCharactersOnReceive <rsconf1_escapecontrolcharactersonreceive.html>`_
-  **$EscapeControlCharactersOnReceive** [**on**\ \|off] - escape
   USASCII HT character
-  $SpaceLFOnReceive [on/**off**] - instructs rsyslogd to replace LF
   with spaces during message reception (sysklogd compatibility aid)
-  $ErrorMessagesToStderr [**on**\ \|off] - direct rsyslogd error
   message to stderr (in addition to other targets)
-  `$FailOnChownFailure <rsconf1_failonchownfailure.html>`_
-  `$FileCreateMode <rsconf1_filecreatemode.html>`_
-  `$FileGroup <rsconf1_filegroup.html>`_
-  `$FileOwner <rsconf1_fileowner.html>`_
-  `$GenerateConfigGraph <rsconf1_generateconfiggraph.html>`_
-  `$GssForwardServiceName <rsconf1_gssforwardservicename.html>`_
-  `$GssListenServiceName <rsconf1_gsslistenservicename.html>`_
-  `$GssMode <rsconf1_gssmode.html>`_
-  `$IncludeConfig <rsconf1_includeconfig.html>`_
-  MainMsgQueueCheckpointInterval <number>
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
-  **$MainMsgQueueDequeueBatchSize** <number> [default 32]
-  $MainMsgQueueDequeueSlowdown <number> [number is timeout in
   *micro*\ seconds (1000000us is 1sec!), default 0 (no delay). Simple
   rate-limiting!]
-  $MainMsgQueueDiscardMark <number> [default 9750]
-  $MainMsgQueueDiscardSeverity <severity> [either a textual or
   numerical severity! default 4 (warning)]
-  $MainMsgQueueFileName <name>
-  $MainMsgQueueHighWaterMark <number> [default 8000]
-  $MainMsgQueueImmediateShutdown [on/**off**]
-  `$MainMsgQueueSize <rsconf1_mainmsgqueuesize.html>`_
-  $MainMsgQueueLowWaterMark <number> [default 2000]
-  $MainMsgQueueMaxFileSize <size\_nbr>, default 1m
-  $MainMsgQueueTimeoutActionCompletion <number> [number is timeout in
   ms (1000ms is 1sec!), default 1000, 0 means immediate!]
-  $MainMsgQueueTimeoutEnqueue <number> [number is timeout in ms (1000ms
   is 1sec!), default 2000, 0 means indefinite]
-  $MainMsgQueueTimeoutShutdown <number> [number is timeout in ms
   (1000ms is 1sec!), default 0 (indefinite)]
-  $MainMsgQueueWorkerTimeoutThreadShutdown <number> [number is timeout
   in ms (1000ms is 1sec!), default 60000 (1 minute)]
-  $MainMsgQueueType [**FixedArray**/LinkedList/Direct/Disk]
-  $MainMsgQueueSaveOnShutdown  [on/**off**]
-  $MainMsgQueueWorkerThreads <number>, num worker threads, default 1,
   recommended 1
-  $MainMsgQueueWorkerThreadMinumumMessages <number>, default 100
-  `$MarkMessagePeriod <rsconf1_markmessageperiod.html>`_ (immark)
-  ***$MaxMessageSize*** <size\_nbr>, default 2k - allows to specify
   maximum supported message size (both for sending and receiving). The
   default should be sufficient for almost all cases. Do not set this
   below 1k, as it would cause interoperability problems with other
   syslog implementations.
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
   Note that 2k, the current default, is the smallest size that must be
   supported in order to be compliant to the upcoming new syslog RFC
   series.
-  `$MaxOpenFiles <rsconf1_maxopenfiles.html>`_
-  `$ModDir <rsconf1_moddir.html>`_
-  `$ModLoad <rsconf1_modload.html>`_
-  **$OMFileAsyncWriting** [on/**off**], if turned on, the files will be
   written in asynchronous mode via a separate thread. In that case,
   double buffers will be used so that one buffer can be filled while
   the other buffer is being written. Note that in order to enable
   $OMFileFlushInterval, $OMFileAsyncWriting must be set to "on".
   Otherwise, the flush interval will be ignored. Also note that when
   $OMFileFlushOnTXEnd is "on" but $OMFileAsyncWriting is off, output
   will only be written when the buffer is full. This may take several
   hours, or even require a rsyslog shutdown. However, a buffer flush
   can be forced in that case by sending rsyslogd a HUP signal.
-  **$OMFileZipLevel** 0..9 [default 0] - if greater 0, turns on gzip
   compression of the output file. The higher the number, the better the
   compression, but also the more CPU is required for zipping.
-  **$OMFileIOBufferSize** <size\_nbr>, default 4k, size of the buffer
   used to writing output data. The larger the buffer, the potentially
   better performance is. The default of 4k is quite conservative, it is
   useful to go up to 64k, and 128K if you used gzip compression (then,
   even higher sizes may make sense)
-  **$OMFileFlushOnTXEnd** <[**on**/off]>, default on. Omfile has the
   capability to write output using a buffered writer. Disk writes are
   only done when the buffer is full. So if an error happens during that
   write, data is potentially lost. In cases where this is unacceptable,
   set $OMFileFlushOnTXEnd to on. Then, data is written at the end of
   each transaction (for pre-v5 this means after **each** log message)
   and the usual error recovery thus can handle write errors without
   data loss. Note that this option severely reduces the effect of zip
   compression and should be switched to off for that use case. Note
   that the default -on- is primarily an aid to preserve the traditional
   syslogd behaviour.
-  `$omfileForceChown <rsconf1_omfileforcechown.html>`_ - force
   ownership change for all files
-  **$RepeatedMsgContainsOriginalMsg** [on/**off**] - "last message
   repeated n times" messages, if generated, have a different format
   that contains the message that is being repeated. Note that only the
   first "n" characters are included, with n to be at least 80
   characters, most probably more (this may change from version to
   version, thus no specific limit is given). The bottom line is that n
   is large enough to get a good idea which message was repeated but it
   is not necessarily large enough for the whole message. (Introduced
   with 4.1.5). Once set, it affects all following actions.
-  `$RepeatedMsgReduction <rsconf1_repeatedmsgreduction.html>`_
-  `$ResetConfigVariables <rsconf1_resetconfigvariables.html>`_
-  **$Ruleset** *name* - starts a new ruleset or switches back to one
   already defined. All following actions belong to that new rule set.
   the *name* does not yet exist, it is created. To switch back to
   rsyslog's default ruleset, specify "RSYSLOG\_DefaultRuleset") as the
   name. All following actions belong to that new rule set. It is
   advised to also read our paper on `using multiple rule sets in
   rsyslog <multi_ruleset.html>`_.
-  **`$RulesetCreateMainQueue <rsconf1_rulesetcreatemainqueue.html>`_**
   on - creates a ruleset-specific main queue.
-  **`$RulesetParser <rsconf1_rulesetparser.html>`_** - enables to set a
   specific (list of) message parsers to be used with the ruleset.
-  **$OptimizeForUniprocessor** [on/**off**] - turns on optimizatons
   which lead to better performance on uniprocessors. If you run on
   multicore-machiens, turning this off lessens CPU load. The default
   may change as uniprocessor systems become less common. [available
   since 4.1.0]
-  $PreserveFQDN [on/**off**) - if set to off (legacy default to remain
   compatible to sysklogd), the domain part from a name that is within
   the same domain as the receiving system is stripped. If set to on,
   full names are always used.
-  $WorkDirectory <name> (directory for spool and other work files. Do
   **not** use trailing slashes)
-  $UDPServerAddress <IP> (imudp) -- local IP address (or name) the UDP
   listens should bind to
-  $UDPServerRun <port> (imudp) -- former -r<port> option, default 514,
   start UDP server on this port, "\*" means all addresses
-  $UDPServerTimeRequery <nbr-of-times> (imudp) -- this is a performance
   optimization. Getting the system time is very costly. With this
   setting, imudp can be instructed to obtain the precise time only once
   every n-times. This logic is only activated if messages come in at a
   very fast rate, so doing less frequent time calls should usually be
   acceptable. The default value is two, because we have seen that even
   without optimization the kernel often returns twice the identical
   time. You can set this value as high as you like, but do so at your
   own risk. The higher the value, the less precise the timestamp.
-  `$PrivDropToGroup <droppriv.html>`_
-  `$PrivDropToGroupID <droppriv.html>`_
-  `$PrivDropToUser <droppriv.html>`_
-  `$PrivDropToUserID <droppriv.html>`_
-  **$Sleep** <seconds> - puts the rsyslog main thread to sleep for the
   specified number of seconds immediately when the directive is
   encountered. You should have a good reason for using this directive!
-  **$LocalHostIPIF** <interface name> - (available since 5.9.6) - if
   provided, the IP of the specified interface (e.g. "eth0") shall be
   used as fromhost-ip for locall-originating messages. If this
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
-  `$UMASK <rsconf1_umask.html>`_

**Where <size\_nbr> or integers are specified above,** modifiers can be
used after the number part. For example, 1k means 1024. Supported are
k(ilo), m(ega), g(iga), t(era), p(eta) and e(xa). Lower case letters
refer to the traditional binary defintion (e.g. 1m equals 1,048,576)
whereas upper case letters refer to their new 1000-based definition (e.g
1M equals 1,000,000).

Numbers may include '.' and ',' for readability. So you can for example
specify either "1000" or "1,000" with the same result. Please note that
rsyslogd simply ignores the punctuation. From it's point of view,
"1,,0.0.,.,0" also has the value 1000.

[`manual index <manual.html>`_\ ]
[`rsyslog.conf <rsyslog_conf.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2010 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
