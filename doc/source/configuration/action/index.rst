Legacy Action-Specific Configuration Statements
===============================================

Statements modify the next action(s) that is/are defined **via legacy syntax**
after the respective statement.
Actions defined via the action() object are **not** affected by the 
legacy statements listed here. Use the action() object properties
instead.

Generic action configuration Statements
---------------------------------------
These statements can be used with all types of actions.

.. toctree::
   :glob:

   *action*
   *rsconf1_repeatedmsgreduction*

-  **$ActionName** <a\_single\_word> - used primarily for documentation,
   e.g. when generating a configuration graph. Available since 4.3.1.
-  **$ActionExecOnlyOnceEveryInterval** <seconds> - execute action only if
   the last execute is at last <seconds> seconds in the past (more info
   in `ommail <ommail.html>`_, but may be used with any action). To
   disable this setting, use value 0.
-  **$ActionExecOnlyEveryNthTime** <number> - If configured, the next
   action will only be executed every n-th time. For example, if
   configured to 3, the first two messages that go into the action will
   be dropped, the 3rd will actually cause the action to execute, the
   4th and 5th will be dropped, the 6th executed under the action, ...
   and so on. Note: this setting is automatically re-set when the actual
   action is defined.
-  **$ActionExecOnlyEveryNthTimeTimeout** <number-of-seconds> - has a
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
-  **$ActionResumeRetryCount** <number> [default 0, -1 means eternal]
-  **$ActionWriteAllMarkMessages** [on/**off**]- [available since 5.1.5]
   - normally, mark messages are written to actions only if the action
   was not recently executed (by default, recently means within the past
   20 minutes). If this setting is switched to "on", mark messages are
   always sent to actions, no matter how recently they have been
   executed. In this mode, mark messages can be used as a kind of
   heartbeat. Note that this option auto-resets to "off", so if you
   intend to use it with multiple actions, it must be specified in front
   off **all** selector lines that should provide this functionality.

omfile-specific Configuration Statements
----------------------------------------
These statements are specific to omfile-based actions.

.. toctree::
   :glob:

   *omfile*
   *dir*
   *file*

-  **$CreateDirs** [**on**/off] - create directories on an as-needed
   basis
-  **$ActionFileDefaultTemplate** [templateName] - sets a new default
   template for file actions
-  **$ActionFileEnableSync [on/off]** - enables file syncing capability of
   omfile
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

omfwd-specific Configuration Statements
---------------------------------------
These statements are specific to omfwd-based actions.

-  **$ActionForwardDefaultTemplate** [templateName] - sets a new default
   template for UDP and plain TCP forwarding action
-  **$ActionSendResendLastMsgOnReconnect** <[on/**off**]> specifies if the
   last message is to be resend when a connection breaks and has been
   reconnected. May increase reliability, but comes at the risk of
   message duplication.
-  **$ActionSendStreamDriver** <driver basename> just like
   $DefaultNetstreamDriver, but for the specific action
-  **$ActionSendStreamDriverMode** <mode>, default 0, mode to use with the
   stream driver (driver-specific)
-  **$ActionSendStreamDriverAuthMode** <mode>,  authentication mode to use
   with the stream driver. Note that this directive requires TLS
   netstream drivers. For all others, it will be ignored.
   (driver-specific)
-  **$ActionSendStreamDriverPermittedPeer** <ID>,  accepted fingerprint
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

omgssapi-specific Configuration Statements
------------------------------------------
These statements are specific to omgssapi actions.

.. toctree::
   :glob:

   *gss*

action-queue specific Configuration Statements
----------------------------------------------
The following statements specify parameters for the action queue.
To understand queue parameters, read
:doc:`queues in rsyslog <../../concepts/queues>`.

Action queue parameters usually affect the next action and auto-reset
to defaults thereafter. Most importantly, this means that when a
"real" (non-direct) queue type is defined, this affects the immediately
following action, only. The next and all other actions will be
in "direct" mode (no real queue) if not explicitly specified otherwise.

-  **$ActionQueueCheckpointInterval** <number>
-  **$ActionQueueDequeueBatchSize** <number> [default 128]
-  **$ActionQueueDequeueSlowdown** <number> [number is timeout in
   *micro*\ seconds (1000000us is 1sec!), default 0 (no delay). Simple
   rate-limiting!]
-  **$ActionQueueDiscardMark** <number> [default 80% of queue size]
-  **$ActionQueueDiscardSeverity** <number> [\*numerical\* severity! default 
   8 (nothing discarded)]
-  **$ActionQueueFileName** <name>
-  **$ActionQueueHighWaterMark** <number> [default 90% of queue size]
-  **$ActionQueueImmediateShutdown** [on/**off**]
-  **$ActionQueueSize** <number>
-  **$ActionQueueLowWaterMark** <number> [default 70% of queue size]
-  **$ActionQueueMaxFileSize** <size\_nbr>, default 1m
-  **$ActionQueueTimeoutActionCompletion** <number> [number is timeout in ms
   (1000ms is 1sec!), default 1000, 0 means immediate!]
-  **$ActionQueueTimeoutEnqueue** <number> [number is timeout in ms (1000ms
   is 1sec!), default 2000, 0 means discard immediately]
-  **$ActionQueueTimeoutShutdown** <number> [number is timeout in ms (1000ms
   is 1sec!), default 0 (indefinite)]
-  **$ActionQueueWorkerTimeoutThreadShutdown** <number> [number is timeout
   in ms (1000ms is 1sec!), default 60000 (1 minute)]
-  **$ActionQueueType** [FixedArray/LinkedList/**Direct**/Disk]
-  **$ActionQueueSaveOnShutdown**  [on/**off**]
-  **$ActionQueueWorkerThreads** <number>, num worker threads, default 1,
   recommended 1
-  $ActionQueueWorkerThreadMinumumMessages <number>, default 100
-  **$ActionGSSForwardDefaultTemplate** [templateName] - sets a new default
   template for GSS-API forwarding action
