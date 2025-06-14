`rsyslog.conf configuration directive <rsyslog_conf_global.html>`_

$RulesetCreateMainQueue
-----------------------

**Type:** ruleset-specific configuration directive

**Parameter Values:** boolean (on/off, yes/no)

**Available since:** 5.3.5+

**Default:** off

**Description:**

Rulesets may use their own "main" message queue for message submission.
Specifying this directive, **inside a ruleset definition**, turns this
on. This is both a performance enhancement and also permits different
rulesets (and thus different inputs within the same rsyslogd instance)
to use different types of main message queues.

The ruleset queue is created with the parameters that are specified for
the main message queue at the time the directive is given. If different
queue configurations are desired, different main message queue
directives must be used **in front of** the $RulesetCreateMainQueue
directive. Note that this directive may only be given once per ruleset.
If multiple statements are specified, only the first is used and for the
others error messages are emitted.

Note that the final set of ruleset configuration directives specifies
the parameters for the default main message queue.

To learn more about this feature, please be sure to read about
`multi-ruleset support in rsyslog <multi_ruleset.html>`_.

**Caveats:**

The configuration statement "$RulesetCreateMainQueue off" has no effect
at all. The capability to specify this is an artifact of the legacy
configuration language.

**Example:**

This example sets up a tcp server with three listeners. Each of these
three listener is bound to a specific ruleset. As a performance
optimization, the rulesets all receive their own private queue. The
result is that received messages can be independently processed. With
only a single main message queue, we would have some lock contention
between the messages. This does not happen here. Note that in this
example, we use different processing. Of course, all messages could also
have been processed in the same way ($IncludeConfig may be useful in
that case!).

::

  $ModLoad imtcp
  # at first, this is a copy of the unmodified rsyslog.conf
  #define rulesets first
  $RuleSet remote10514
  $RulesetCreateMainQueue on # create ruleset-specific queue
  *.*     /var/log/remote10514
  
  $RuleSet remote10515
  $RulesetCreateMainQueue on # create ruleset-specific queue
  *.*     /var/log/remote10515
  
  $RuleSet remote10516
  $RulesetCreateMainQueue on # create ruleset-specific queue
  mail.*	/var/log/mail10516
  &       ~
  # note that the discard-action will prevent this message from
  # being written to the remote10516 file - as usual...
  *.*     /var/log/remote10516
  
  # and now define listeners bound to the relevant ruleset
  $InputTCPServerBindRuleset remote10514
  $InputTCPServerRun 10514
  
  $InputTCPServerBindRuleset remote10515
  $InputTCPServerRun 10515
  
  $InputTCPServerBindRuleset remote10516
  $InputTCPServerRun 10516


Note the positions of the directives. With the legacy language,
position is very important. It is highly suggested to use
the *ruleset()* object in RainerScript config language if you intend
to use ruleset queues. The configuration is much more straightforward in
that language and less error-prone.

