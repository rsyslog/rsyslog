Legacy Format Samples for Multiple Rulesets
===========================================

This chapter complements rsyslog's documentation of rulesets.
While the base document focusses on RainerScript format, it
does not provide samples in legacy format. These are included
in this document.

**Important:** do **not** use legacy ruleset definitions for new
configurations. Especially with rulesets, legacy format is extremely
hard to get right. The information in this page is included in order
to help you understand already existing configurations using the
ruleset feature. We even recommend to convert any such configs
to RainerScript format because of its increased robustness
and simplicity.

Legacy ruleset support was available starting with version 4.5.0
and 5.1.1.

Split local and remote logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let's say you have a pretty standard system that logs its local messages
to the usual bunch of files that are specified in the default
rsyslog.conf. As an example, your rsyslog.conf might look like this:

::

    # ... module loading ...
    # The authpriv file has restricted access.
    authpriv.*  /var/log/secure
    # Log all the mail messages in one place.
    mail.*      /var/log/maillog
    # Log cron stuff
    cron.*      /var/log/cron
    # Everybody gets emergency messages
    *.emerg     *
    ... more ...

Now, you want to add receive messages from a remote system and log these
to a special file, but you do not want to have these messages written to
the files specified above. The traditional approach is to add a rule in
front of all others that filters on the message, processes it and then
discards it:

::

    # ... module loading ...
    # process remote messages
    :fromhost-ip, isequal, "192.0.2.1"    /var/log/remotefile
    & ~
    # only messages not from 192.0.21 make it past this point

    # The authpriv file has restricted access.
    authpriv.*                            /var/log/secure
    # Log all the mail messages in one place.
    mail.*                                /var/log/maillog
    # Log cron stuff
    cron.*                                /var/log/cron
    # Everybody gets emergency messages
    *.emerg                               *
    ... more ...

Note the tilde character, which is the discard action!. Also note that
we assume that 192.0.2.1 is the sole remote sender (to keep it simple).

With multiple rulesets, we can simply define a dedicated ruleset for the
remote reception case and bind it to the receiver. This may be written
as follows:

::

    # ... module loading ...
    # process remote messages
    # define new ruleset and add rules to it:
    $RuleSet remote
    *.*           /var/log/remotefile
    # only messages not from 192.0.21 make it past this point

    # bind ruleset to tcp listener
    $InputTCPServerBindRuleset remote
    # and activate it:
    $InputTCPServerRun 10514

    # switch back to the default ruleset:
    $RuleSet RSYSLOG_DefaultRuleset
    # The authpriv file has restricted access.
    authpriv.*    /var/log/secure
    # Log all the mail messages in one place.
    mail.*        /var/log/maillog
    # Log cron stuff
    cron.*        /var/log/cron
    # Everybody gets emergency messages
    *.emerg       *
    ... more ...

Here, we need to switch back to the default ruleset after we have
defined our custom one. This is why I recommend a different ordering,
which I find more intuitive. The sample below has it, and it leads to
the same results:

::

    # ... module loading ...
    # at first, this is a copy of the unmodified rsyslog.conf
    # The authpriv file has restricted access.
    authpriv.*    /var/log/secure
    # Log all the mail messages in one place.
    mail.*        /var/log/maillog
    # Log cron stuff
    cron.*        /var/log/cron
    # Everybody gets emergency messages
    *.emerg       *
    ... more ...
    # end of the "regular" rsyslog.conf. Now come the new definitions:

    # process remote messages
    # define new ruleset and add rules to it:
    $RuleSet remote
    *.*           /var/log/remotefile

    # bind ruleset to tcp listener
    $InputTCPServerBindRuleset remote
    # and activate it:
    $InputTCPServerRun 10514

Here, we do not switch back to the default ruleset, because this is not
needed as it is completely defined when we begin the "remote" ruleset.

Now look at the examples and compare them to the single-ruleset
solution. You will notice that we do **not** need a real filter in the
multi-ruleset case: we can simply use "\*.\*" as all messages now means
all messages that are being processed by this rule set and all of them
come in via the TCP receiver! This is what makes using multiple rulesets
so much easier.

Split local and remote logging for three different ports
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This example is almost like the first one, but it extends it a little
bit. While it is very similar, I hope it is different enough to provide
a useful example why you may want to have more than two rulesets.

Again, we would like to use the "regular" log files for local logging,
only. But this time we set up three syslog/tcp listeners, each one
listening to a different port (in this example 10514, 10515, and 10516).
Logs received from these receivers shall go into different files. Also,
logs received from 10516 (and only from that port!) with "mail.\*"
priority, shall be written into a specif file and **not** be written to
10516's general log file.

This is the config:

::

    # ... module loading ...
    # at first, this is a copy of the unmodified rsyslog.conf
    # The authpriv file has restricted access.
    authpriv.* /var/log/secure
    # Log all the mail messages in one place.
    mail.*  /var/log/maillog
    # Log cron stuff
    cron.*  /var/log/cron
    # Everybody gets emergency messages
    *.emerg       *
    ... more ...
    # end of the "regular" rsyslog.conf. Now come the new definitions:

    # process remote messages

    #define rulesets first
    $RuleSet remote10514
    *.*     /var/log/remote10514

    $RuleSet remote10515
    *.*     /var/log/remote10515

    $RuleSet remote10516
    mail.*  /var/log/mail10516
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

Note that the "mail.\*" rule inside the "remote10516" ruleset does not
affect processing inside any other rule set, including the default rule
set.

